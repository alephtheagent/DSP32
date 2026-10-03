/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _HW_CONFIG_H_
#define _HW_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// New prototype hardware defaults (shifted by -1)
#define HW_DEFAULT_I2S_BCK_PIN     10
#define HW_DEFAULT_I2S_DIN_PIN     11
#define HW_DEFAULT_I2S_WS_PIN      12
#define HW_DEFAULT_NEOPIXEL_PIN    48
#define HW_DEFAULT_BOOT_BUTTON_PIN 0
#define HW_DEFAULT_WIFI_SSID       "ESP32-DSP-DAC"
#define HW_DEFAULT_WIFI_PASS       "12345678"
#define HW_DEFAULT_WIFI_CHANNEL    1

typedef struct {
    int8_t i2s_bck_gpio;
    int8_t i2s_din_gpio;
    int8_t i2s_ws_gpio;
    int8_t neopixel_gpio;
    int8_t boot_button_gpio;
    char wifi_ssid[32];
    char wifi_pass[64];
    uint8_t wifi_channel;
} hw_config_t;

esp_err_t hw_config_init(void);
const hw_config_t *hw_config_get(void);
esp_err_t hw_config_set(const hw_config_t *cfg);
esp_err_t hw_config_reset_defaults(void);

#ifdef __cplusplus
}
#endif

#endif /* _HW_CONFIG_H_ */
