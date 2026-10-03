/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _OLED_DISPLAY_H_
#define _OLED_DISPLAY_H_

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the SSD1306 OLED display using u8g2 library.
 * Pins are read from hw_config (default SCL=8, SDA=9).
 * Starts an asynchronous FreeRTOS task on Core 0 (~25 FPS).
 */
esp_err_t oled_display_init(void);

/**
 * @brief Check if the OLED display was detected and is running.
 */
bool oled_display_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* _OLED_DISPLAY_H_ */
