/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DNS_CAPTIVE_H_
#define _DNS_CAPTIVE_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t dns_captive_start(void);
void dns_captive_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* _DNS_CAPTIVE_H_ */
