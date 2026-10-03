/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _NEOPIXEL_H_
#define _NEOPIXEL_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NEOPIXEL_GPIO_NUM   48

typedef enum {
    NEOPIXEL_MODE_OFF = 0,
    NEOPIXEL_MODE_VU_PEAK,      // Classic Traffic Light VU: Green -> Yellow -> Red
    NEOPIXEL_MODE_BASS_PULSE,    // Punchy Bass/Beat pulse (Deep Blue -> Magenta -> Cyan)
    NEOPIXEL_MODE_RAINBOW,      // Spectrum cycle with RMS energy reaction
    NEOPIXEL_MODE_TUBE_FIRE,    // Warm analog tube filament glow & dynamic flicker
    NEOPIXEL_MODE_STATUS,       // Minimalist audio/wifi status indicator
    NEOPIXEL_MODE_COUNT
} neopixel_mode_t;

typedef struct {
    neopixel_mode_t mode;
    uint8_t brightness; // 0 - 100%
} neopixel_config_t;

esp_err_t neopixel_init(void);
void neopixel_set_config(const neopixel_config_t *cfg);
void neopixel_get_config(neopixel_config_t *out_cfg);
void neopixel_set_mode(neopixel_mode_t mode);
void neopixel_set_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif /* _NEOPIXEL_H_ */
