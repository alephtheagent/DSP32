/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _WIFI_AP_H_
#define _WIFI_AP_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_AP_SSID            "DSP32-DAC"
#define WIFI_AP_CHANNEL         6
#define WIFI_AP_MAX_CONN        4
#define WIFI_AP_AUTO_OFF_SEC    30

typedef void (*wifi_state_changed_cb_t)(bool is_active);

esp_err_t wifi_ap_init(wifi_state_changed_cb_t cb);
esp_err_t wifi_ap_start(void);
esp_err_t wifi_ap_stop(void);
bool wifi_ap_is_active(void);
uint8_t wifi_ap_get_station_count(void);
void wifi_ap_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIFI_AP_H_ */
