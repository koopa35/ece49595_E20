// main/read_write_main.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

static const char *TAG = "RUNTIME_LOG";

// I2C pins
#define I2C_PORT     I2C_NUM_0
#define I2C_SDA_GPIO 21
#define I2C_SCL_GPIO 22
#define I2C_FREQ_HZ  50000   // 50k is slowed down, can change to 10k once working 

// Can adjust EEPROM adress by changing which we pull high A0-A2 on breadboard 
#define EEPROM_ADDR  0x57
#define EEPROM_SIZE_BYTES 4096
#define EEPROM_MAX_ADDR   (EEPROM_SIZE_BYTES - 1)

#define EEPROM_PAGE_SIZE  32

// Can adjust depending on how often we want to update 
#define SAVE_INTERVAL_MINUTES 1

// ==================== SIMPLE CRC32 (FNV-1a 32-bit) ====================
static uint32_t fnv1a32(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

// ==================== I2C INIT ====================
static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_GPIO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
        .clk_flags = 0,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0));
}

// EEPROM HELPERS

static esp_err_t eeprom_wait_ready(void)
{
    // something about NULL buffers not working with ESP-IDF v5 so using a byte dummy write.
    uint8_t dummy = 0x00;

    for (int i = 0; i < 60; i++) { 
        esp_err_t err = i2c_master_write_to_device(
            I2C_PORT, EEPROM_ADDR, &dummy, 1, pdMS_TO_TICKS(10)
        );
        if (err == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t eeprom_write_bytes(uint16_t mem_addr, const uint8_t *data, size_t len)
{
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if ((uint32_t)mem_addr + len - 1 > EEPROM_MAX_ADDR) return ESP_ERR_INVALID_ARG;

    // Enforce not crossing a page boundary in a single write 
    uint16_t page_off = mem_addr % EEPROM_PAGE_SIZE;
    if (page_off + len > EEPROM_PAGE_SIZE) return ESP_ERR_INVALID_ARG;

    // Buffer: 2-byte address + data
    uint8_t buf[2 + EEPROM_PAGE_SIZE];
    buf[0] = (uint8_t)(mem_addr >> 8);
    buf[1] = (uint8_t)(mem_addr & 0xFF);
    memcpy(&buf[2], data, len);

    esp_err_t err = i2c_master_write_to_device(
        I2C_PORT, EEPROM_ADDR, buf, 2 + len, pdMS_TO_TICKS(1000)
    );
    if (err != ESP_OK) return err;

    return eeprom_wait_ready();
}

static esp_err_t eeprom_read_bytes(uint16_t mem_addr, uint8_t *out, size_t len)
{
    if (!out || len == 0) return ESP_ERR_INVALID_ARG;
    if ((uint32_t)mem_addr + len - 1 > EEPROM_MAX_ADDR) return ESP_ERR_INVALID_ARG;

    uint8_t addr[2] = {
        (uint8_t)(mem_addr >> 8),
        (uint8_t)(mem_addr & 0xFF)
    };

    return i2c_master_write_read_device(
        I2C_PORT, EEPROM_ADDR,
        addr, sizeof(addr),
        out, len,
        pdMS_TO_TICKS(1000)
    );
}


// Keep record <= 32 bytes so it fits in one page write
typedef struct __attribute__((packed)) {
    uint32_t magic;          // identifies valid records
    uint32_t seq;            // increasing counter for newest selection
    uint32_t total_minutes;  // runtime in minutes
    uint32_t crc;           
} runtime_record_t;

#define REC_MAGIC 0xA5C3F00Du
#define REC_SIZE  ((uint16_t)sizeof(runtime_record_t))

#define SLOT_COUNT (EEPROM_SIZE_BYTES / REC_SIZE)

// Start storing at address 0
#define SLOT_ADDR(slot) ((uint16_t)((slot) * REC_SIZE))

_Static_assert((EEPROM_SIZE_BYTES % REC_SIZE) == 0, "Record size must evenly divide EEPROM size");
_Static_assert(REC_SIZE <= EEPROM_PAGE_SIZE, "Record must fit within one EEPROM page write");

// Validate record (magic + crc)
static bool record_valid(const runtime_record_t *r)
{
    if (r->magic != REC_MAGIC) return false;

    runtime_record_t tmp = *r;
    tmp.crc = 0;
    uint32_t c = fnv1a32(&tmp, sizeof(tmp));
    return (c == r->crc);
}

// Read record at slot index
static esp_err_t record_read_slot(uint16_t slot, runtime_record_t *out)
{
    if (!out || slot >= SLOT_COUNT) return ESP_ERR_INVALID_ARG;
    return eeprom_read_bytes(SLOT_ADDR(slot), (uint8_t *)out, sizeof(*out));
}

// Write record at slot index 
static esp_err_t record_write_slot(uint16_t slot, const runtime_record_t *rec)
{
    if (!rec || slot >= SLOT_COUNT) return ESP_ERR_INVALID_ARG;

    // Ensure write doesn't cross page boundary
    uint16_t addr = SLOT_ADDR(slot);
    if ((addr % EEPROM_PAGE_SIZE) + sizeof(*rec) > EEPROM_PAGE_SIZE) return ESP_ERR_INVALID_STATE;

    return eeprom_write_bytes(addr, (const uint8_t *)rec, sizeof(*rec));
}

// Find newest valid record in EEPROM
static void load_latest_record(uint32_t *out_minutes, uint32_t *out_seq, uint16_t *out_slot)
{
    uint32_t best_seq = 0;
    uint32_t best_minutes = 0;
    uint16_t best_slot = 0;
    bool found = false;

    runtime_record_t r;
    for (uint16_t slot = 0; slot < SLOT_COUNT; slot++) {
        if (record_read_slot(slot, &r) != ESP_OK) continue;
        if (!record_valid(&r)) continue;

        if (!found || r.seq > best_seq) {
            best_seq = r.seq;
            best_minutes = r.total_minutes;
            best_slot = slot;
            found = true;
        }
    }

    if (!found) {
        *out_minutes = 0;
        *out_seq = 0;
        *out_slot = 0;
        ESP_LOGW(TAG, "No valid runtime record found. Starting at 0 minutes.");
    } else {
        *out_minutes = best_minutes;
        *out_seq = best_seq;
        *out_slot = best_slot;
        ESP_LOGI(TAG, "Loaded runtime: %" PRIu32 " minutes (seq=%" PRIu32 ", slot=%u)",
                 best_minutes, best_seq, best_slot);
    }
}

static esp_err_t save_next_record(uint16_t *io_slot, uint32_t *io_seq, uint32_t total_minutes)
{
    uint16_t next_slot = (uint16_t)((*io_slot + 1) % SLOT_COUNT);
    uint32_t next_seq = *io_seq + 1;

    runtime_record_t rec = {
        .magic = REC_MAGIC,
        .seq = next_seq,
        .total_minutes = total_minutes,
        .crc = 0
    };
    rec.crc = fnv1a32(&rec, sizeof(rec));

    esp_err_t err = record_write_slot(next_slot, &rec);
    if (err == ESP_OK) {
        *io_slot = next_slot;
        *io_seq = next_seq;
        ESP_LOGI(TAG, "Saved: %" PRIu32 " minutes (seq=%" PRIu32 ", slot=%u)",
                 total_minutes, next_seq, next_slot);
    }
    return err;
}

// store minutes in EEPROM. During runtime we track elapsed time using esp_timer
static uint32_t g_base_minutes = 0;         // loaded from EEPROM at boot
static uint32_t g_last_saved_minutes = 0;   // last value written to EEPROM
static uint32_t g_seq = 0;
static uint16_t g_slot = 0;
static int64_t  g_boot_us = 0;

static uint32_t current_total_minutes(void)
{
    int64_t now_us = esp_timer_get_time();
    int64_t elapsed_us = now_us - g_boot_us;
    uint32_t elapsed_min = (uint32_t)(elapsed_us / (60LL * 1000000LL));
    return g_base_minutes + elapsed_min;
}

void app_main(void)
{
    i2c_master_init();

    // Load newest record from EEPROM
    load_latest_record(&g_base_minutes, &g_seq, &g_slot);
    g_last_saved_minutes = g_base_minutes;

    g_boot_us = esp_timer_get_time();

    ESP_LOGI(TAG, "Wear-level slots: %u (record size %u bytes, save interval %u min)",
             (unsigned)SLOT_COUNT, (unsigned)REC_SIZE, (unsigned)SAVE_INTERVAL_MINUTES);

    while (1) {
        uint32_t total_min = current_total_minutes();

        // Log occasionally to see it moving; once a minute
        static uint32_t last_print = 0;
        if (total_min != last_print) {
            last_print = total_min;
            ESP_LOGI(TAG, "Total Runtime : %" PRIu32 " minutes", total_min);
        }

        // Save every SAVE_INTERVAL_MINUTES using wear leveling
        if ((total_min - g_last_saved_minutes) >= SAVE_INTERVAL_MINUTES) {
            esp_err_t err = save_next_record(&g_slot, &g_seq, total_min);
            if (err == ESP_OK) {
                g_last_saved_minutes = total_min;
            } else {
                ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(err));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
