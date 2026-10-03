/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "boot_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "wifi/wifi_ap.h"
#include "storage/hw_config.h"

static const char *TAG = "BOOT_BTN";
static int s_button_gpio = BOOT_BUTTON_GPIO;

static void button_task(void *arg)
{
    bool last_state = true;
    while (1) {
        int level = gpio_get_level((gpio_num_t)s_button_gpio);
        // Active LOW button
        if (level == 0 && last_state == true) {
            // Debounce delay
            vTaskDelay(pdMS_TO_TICKS(50));
            if (gpio_get_level((gpio_num_t)s_button_gpio) == 0) {
                ESP_LOGI(TAG, "Boot button pressed -> Toggling Wi-Fi SoftAP");
                wifi_ap_toggle();
                // Wait for release
                while (gpio_get_level((gpio_num_t)s_button_gpio) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        last_state = (level != 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t boot_button_init(void)
{
    const hw_config_t *hw = hw_config_get();
    s_button_gpio = (hw && hw->boot_button_gpio >= 0) ? hw->boot_button_gpio : BOOT_BUTTON_GPIO;
    if (s_button_gpio < 0) {
        ESP_LOGI(TAG, "Boot button disabled in hardware config");
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_button_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;

    BaseType_t r = xTaskCreatePinnedToCore(button_task, "btn_task", 4096, NULL, 3, NULL, 0);
    return (r == pdPASS) ? ESP_OK : ESP_FAIL;
}
