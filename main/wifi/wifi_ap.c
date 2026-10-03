/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "wifi_ap.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "storage/hw_config.h"

static const char *TAG = "WIFI_AP";

static esp_netif_t *s_ap_netif = NULL;
static bool s_is_active = false;
static uint8_t s_connected_stations = 0;
static esp_timer_handle_t s_auto_off_timer = NULL;
static wifi_state_changed_cb_t s_state_cb = NULL;

static void auto_off_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "No stations connected for %d seconds. Auto-shutting down Wi-Fi to eliminate RF interference!", WIFI_AP_AUTO_OFF_SEC);
    wifi_ap_stop();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "WIFI_EVENT_AP_START -> starting network services");
            if (s_state_cb) {
                s_state_cb(true);
            }
        } else if (event_id == WIFI_EVENT_AP_STOP) {
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP -> stopping network services");
            if (s_state_cb) {
                s_state_cb(false);
            }
        } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            s_connected_stations++;
            ESP_LOGI(TAG, "Station connected (MAC: " MACSTR ", AID=%d), Total: %d",
                     MAC2STR(event->mac), event->aid, s_connected_stations);

            // Cancel auto-off timer if running
            if (s_auto_off_timer) {
                esp_timer_stop(s_auto_off_timer);
            }
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            if (s_connected_stations > 0) s_connected_stations--;
            ESP_LOGI(TAG, "Station disconnected (MAC: " MACSTR ", AID=%d), Remaining: %d",
                     MAC2STR(event->mac), event->aid, s_connected_stations);

            if (s_connected_stations == 0 && s_is_active) {
                ESP_LOGI(TAG, "All stations disconnected. Starting %d-second auto-off timer...", WIFI_AP_AUTO_OFF_SEC);
                if (s_auto_off_timer) {
                    esp_timer_start_once(s_auto_off_timer, (uint64_t)WIFI_AP_AUTO_OFF_SEC * 1000000ULL);
                }
            }
        }
    }
}

esp_err_t wifi_ap_init(wifi_state_changed_cb_t cb)
{
    s_state_cb = cb;

    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);

    esp_timer_create_args_t timer_args = {
        .callback = &auto_off_timer_cb,
        .name = "wifi_auto_off"
    };
    esp_timer_create(&timer_args, &s_auto_off_timer);

    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    const hw_config_t *hw = hw_config_get();
    const char *ssid = (hw && strlen(hw->wifi_ssid) > 0) ? hw->wifi_ssid : WIFI_AP_SSID;
    const char *pass = (hw && strlen(hw->wifi_pass) > 0) ? hw->wifi_pass : "";
    uint8_t channel = (hw && hw->wifi_channel >= 1 && hw->wifi_channel <= 13) ? hw->wifi_channel : WIFI_AP_CHANNEL;

    wifi_config_t wifi_config = {
        .ap = {
            .channel = channel,
            .max_connection = WIFI_AP_MAX_CONN,
        },
    };
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(ssid);
    if (strlen(pass) >= 8) {
        strncpy((char *)wifi_config.ap.password, pass, sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);

    ESP_LOGI(TAG, "Wi-Fi initialized in AP mode (SSID: '%s', Auth: %s, Radio OFF, press BOOT to start)",
             ssid, wifi_config.ap.authmode == WIFI_AUTH_OPEN ? "OPEN" : "WPA2-PSK");
    return ESP_OK;
}

esp_err_t wifi_ap_start(void)
{
    if (s_is_active) return ESP_OK;

    s_is_active = true;
    s_connected_stations = 0;

    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        s_is_active = false;
        return ret;
    }

    // Start auto-off timer initially in case no client connects within timeout
    if (s_auto_off_timer) {
        esp_timer_start_once(s_auto_off_timer, (uint64_t)WIFI_AP_AUTO_OFF_SEC * 2 * 1000000ULL);
    }

    ESP_LOGI(TAG, "SoftAP started: SSID='%s' IP=192.168.4.1", WIFI_AP_SSID);
    return ESP_OK;
}

esp_err_t wifi_ap_stop(void)
{
    if (!s_is_active) return ESP_OK;

    if (s_auto_off_timer) {
        esp_timer_stop(s_auto_off_timer);
    }

    s_is_active = false;
    s_connected_stations = 0;

    esp_wifi_stop();

    ESP_LOGI(TAG, "SoftAP stopped (Wi-Fi Radio OFF, RF noise eliminated)");
    return ESP_OK;
}

bool wifi_ap_is_active(void)
{
    return s_is_active;
}

uint8_t wifi_ap_get_station_count(void)
{
    return s_connected_stations;
}

void wifi_ap_toggle(void)
{
    if (s_is_active) {
        wifi_ap_stop();
    } else {
        wifi_ap_start();
    }
}
