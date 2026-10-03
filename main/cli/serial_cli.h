/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _SERIAL_CLI_H_
#define _SERIAL_CLI_H_

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB CDC Serial CLI / API engine
 */
esp_err_t serial_cli_init(void);

/**
 * @brief Set terminal echo mode
 * @param enable true for interactive terminal (echo + prompt), false for machine JSON mode
 */
void serial_cli_set_echo(bool enable);

/**
 * @brief Get terminal echo mode
 */
bool serial_cli_get_echo(void);

#ifdef __cplusplus
}
#endif

#endif /* _SERIAL_CLI_H_ */
