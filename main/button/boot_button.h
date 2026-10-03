/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _BOOT_BUTTON_H_
#define _BOOT_BUTTON_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_BUTTON_GPIO 0

esp_err_t boot_button_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _BOOT_BUTTON_H_ */
