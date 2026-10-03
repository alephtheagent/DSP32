/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _WEB_SERVER_H_
#define _WEB_SERVER_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_server_start(void);
void web_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* _WEB_SERVER_H_ */
