
#include "runtime_log.h"

#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t seq;
    uint32_t total_minutes;
    uint32_t crc;
} runtime_record_t;

#define REC_MAGIC 0xA5C3F00Du
#define REC_SIZE  ((uint16_t)sizeof(runtime_record_t))
#define RUNTIME_TASK_STACK_WORDS 4096   // 4096 words = 16 KB 
#define RUNTIME_TASK_PRIORITY    5


static const char *TAG = "RUNTIME_LOG";

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

// Validate record 
static bool record_valid(const runtime_record_t *r)
{
    if (r->magic != REC_MAGIC) return false;

    runtime_record_t tmp = *r;
    tmp.crc = 0;
    uint32_t c = fnv1a32(&tmp, sizeof(tmp));
    return (c == r->crc);
}

static inline uint16_t slot_addr(uint16_t slot)
{
    return (uint16_t)(slot * REC_SIZE);
}

static esp_err_t record_read_slot(const runtime_log_t *h, uint16_t slot, runtime_record_t *out)
{
    if (!h || !out || slot >= h->slot_count) return ESP_ERR_INVALID_ARG;
    return eeprom_24xx_read_bytes(h->eeprom, slot_addr(slot), (uint8_t *)out, sizeof(*out));
}

static esp_err_t record_write_slot(const runtime_log_t *h, uint16_t slot, const runtime_record_t *rec)
{
    if (!h || !rec || slot >= h->slot_count) return ESP_ERR_INVALID_ARG;

    uint16_t addr = slot_addr(slot);

    // Ensure write doesn't cross page boundary 
    if ((addr % h->eeprom->page_size) + sizeof(*rec) > h->eeprom->page_size) {
        return ESP_ERR_INVALID_STATE;
    }

    return eeprom_24xx_write_bytes(h->eeprom, addr, (const uint8_t *)rec, sizeof(*rec));
}

// Find newest valid record in EEPROM 
static void load_latest_record(runtime_log_t *h)
{
    uint32_t best_seq = 0;
    uint32_t best_minutes = 0;
    uint16_t best_slot = 0;
    bool found = false;

    runtime_record_t r;
    for (uint16_t slot = 0; slot < h->slot_count; slot++) {
        if (record_read_slot(h, slot, &r) != ESP_OK) continue;
        if (!record_valid(&r)) continue;

        if (!found || r.seq > best_seq) {
            best_seq = r.seq;
            best_minutes = r.total_minutes;
            best_slot = slot;
            found = true;
        }
    }

    if (!found) {
        h->base_minutes = 0;
        h->seq = 0;
        h->slot = 0;
        ESP_LOGW(TAG, "No valid runtime record found. Starting at 0 minutes.");
    } else {
        h->base_minutes = best_minutes;
        h->seq = best_seq;
        h->slot = best_slot;
        ESP_LOGI(TAG, "Loaded runtime: %" PRIu32 " minutes (seq=%" PRIu32 ", slot=%u)",
                 best_minutes, best_seq, best_slot);
    }

    h->last_saved_minutes = h->base_minutes;
}

static esp_err_t save_next_record(runtime_log_t *h, uint32_t total_minutes)
{
    uint16_t next_slot = (uint16_t)((h->slot + 1) % h->slot_count);
    uint32_t next_seq  = h->seq + 1;

    runtime_record_t rec = {
        .magic = REC_MAGIC,
        .seq = next_seq,
        .total_minutes = total_minutes,
        .crc = 0
    };
    rec.crc = fnv1a32(&rec, sizeof(rec));

    esp_err_t err = record_write_slot(h, next_slot, &rec);
    if (err == ESP_OK) {
        h->slot = next_slot;
        h->seq  = next_seq;
        ESP_LOGI(TAG, "Saved: %" PRIu32 " minutes (seq=%" PRIu32 ", slot=%u)",
                 total_minutes, next_seq, next_slot);
    }
    return err;
}

esp_err_t runtime_log_init(runtime_log_t *h, const eeprom_24xx_t *eeprom, const runtime_log_cfg_t *cfg)
{
    if (!h || !eeprom || !cfg) return ESP_ERR_INVALID_ARG;
    if (REC_SIZE > eeprom->page_size) return ESP_ERR_INVALID_STATE;
    if ((eeprom->size_bytes % REC_SIZE) != 0) return ESP_ERR_INVALID_STATE;

    memset(h, 0, sizeof(*h));
    h->eeprom = eeprom;
    h->cfg = *cfg;
    h->slot_count = (uint16_t)(eeprom->size_bytes / REC_SIZE);

    return ESP_OK;
}

esp_err_t runtime_log_load_latest(runtime_log_t *h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    load_latest_record(h);
    return ESP_OK;
}

esp_err_t runtime_log_set_boot_time_now(runtime_log_t *h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    h->boot_us = esp_timer_get_time();
    return ESP_OK;
}

uint32_t runtime_log_current_total_minutes(const runtime_log_t *h)
{
    int64_t now_us = esp_timer_get_time();
    int64_t elapsed_us = now_us - h->boot_us;
    uint32_t elapsed_min = (uint32_t)(elapsed_us / (60LL * 1000000LL));
    return h->base_minutes + elapsed_min;
}

esp_err_t runtime_log_maybe_save(runtime_log_t *h)
{
    if (!h) return ESP_ERR_INVALID_ARG;

    uint32_t total_min = runtime_log_current_total_minutes(h);

    if ((total_min - h->last_saved_minutes) >= h->cfg.save_interval_minutes) {
        esp_err_t err = save_next_record(h, total_min);
        if (err == ESP_OK) {
            h->last_saved_minutes = total_min;
        }
        return err;
    }

    return ESP_OK;
}

static void runtime_task(void *arg)
{
    runtime_log_t *h = (runtime_log_t *)arg;

    uint32_t last_print = UINT32_MAX;

    while (1) {
        uint32_t total_min = runtime_log_current_total_minutes(h);

        // Print when minutes changes 
        if (total_min != last_print) {
            last_print = total_min;
            ESP_LOGI(TAG, "Total Runtime : %" PRIu32 " minutes", total_min);
        }

        // Save periodically
        esp_err_t err = runtime_log_maybe_save(h);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

esp_err_t runtime_log_start_task(runtime_log_t *h, uint32_t stack_words, UBaseType_t priority)
{
    if (!h || stack_words == 0) return ESP_ERR_INVALID_ARG;

    // Pin to CPU0 to avoid early Core1 scheduling issues while debugging
    BaseType_t ok = xTaskCreatePinnedToCore(
        runtime_task,
        "runtime_task",
        stack_words,   // stack depth in WORDS
        h,
        priority,
        NULL,
        0              // core 0
    );

    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}


