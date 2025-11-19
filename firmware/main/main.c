/* SPDX-License-Identifier: Unlicense */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "WT32_AT";

#define UART_PORT_NUM      UART_NUM_1
#define UART_TX_PIN        17     // ESP32 TX → WT32 RX
#define UART_RX_PIN        16     // ESP32 RX ← WT32 TX
#define UART_BAUD_RATE     115200
#define BUF_SIZE           1024

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "UART initialized. Sending AT commands...");

    const char *at_cmd = "AT\r\n"; // send CR+LF
    uint8_t data[BUF_SIZE];

    while (1) {
        // Send AT command
        int len = uart_write_bytes(UART_PORT_NUM, at_cmd, strlen(at_cmd));
        ESP_LOGI(TAG, "Sent %d bytes: %s", len, at_cmd);

        // Wait 100ms
        vTaskDelay(pdMS_TO_TICKS(100));

        // Read response
        int rx_bytes = uart_read_bytes(UART_PORT_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(500));
        if (rx_bytes > 0) {
            data[rx_bytes] = '\0'; // null-terminate
            ESP_LOGI(TAG, "Received %d bytes: %s", rx_bytes, (char *)data);
        } else {
            ESP_LOGW(TAG, "No response received");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // repeat every 1s
    }
}