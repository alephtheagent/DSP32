/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"
#include "mdns.h"

#include "i2s_dac.h"
#include "usb_descriptors.h"
#include "usb_audio.h"
#include "audio_pipeline.h"
#include "storage/preset_manager.h"
#include "storage/hw_config.h"
#include "storage/fs_manager.h"
#include "ota/ota_manager.h"
#include "wifi/wifi_ap.h"
#include "wifi/dns_captive.h"
#include "web/web_server.h"
#include "cli/serial_cli.h"
#include "button/boot_button.h"
#include "neopixel.h"

static const char *TAG = "MAIN";

static usb_phy_handle_t s_phy_hdl = NULL;

static void on_sample_rate_changed(uint32_t new_rate, uint8_t new_depth)
{
    ESP_LOGI(TAG, "Host requested format update: %" PRIu32 " Hz, %d-bit", new_rate, new_depth);
    audio_pipeline_set_format(new_rate, new_depth);
}

static bool s_mdns_initialized = false;

static void on_wifi_state_changed(bool is_active)
{
    if (is_active) {
        ESP_LOGI(TAG, "Wi-Fi AP active -> Starting mDNS, Captive Portal DNS & Web Server");
        esp_err_t mdns_err = mdns_init();
        if (mdns_err == ESP_OK) {
            s_mdns_initialized = true;
            mdns_hostname_set("dsp32");
            mdns_instance_name_set("DSP32-S3 Hi-Fi DAC");
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        }
        dns_captive_start();
        web_server_start();
    } else {
        ESP_LOGI(TAG, "Wi-Fi AP stopped -> Stopping Captive Portal DNS & Web Server");
        dns_captive_stop();
        web_server_stop();
        if (s_mdns_initialized) {
            mdns_free();
            s_mdns_initialized = false;
        }
    }
}

static void usb_device_task(void *pvParameters)
{
    ESP_LOGI(TAG, "TinyUSB Task started on Core %d", xPortGetCoreID());
    while (1) {
        tud_task_ext(portMAX_DELAY, false);
    }
}

static void init_usb_hardware(void)
{
    // Configure ESP32-S3 internal USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .target = USB_PHY_TARGET_INT,
    };
    esp_err_t ret = usb_new_phy(&phy_conf, &s_phy_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB PHY: %s", esp_err_to_name(ret));
        return;
    }

    // Ensure descriptor callbacks are linked
    usb_descriptors_init();

    // Initialize TinyUSB stack
    bool ok = tusb_init();
    if (!ok) {
        ESP_LOGE(TAG, "Failed to initialize TinyUSB stack");
        return;
    }

    // TinyUSB task on Core 1 (Dedicated Audio I/O Core) with MAX real-time priority
    xTaskCreatePinnedToCore(usb_device_task, "tusb_task", 4096, NULL, configMAX_PRIORITIES - 1, NULL, 1);
    ESP_LOGI(TAG, "USB OTG Device Stack initialized on Core 1 (Priority %d)", configMAX_PRIORITIES - 1);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " ESP32-S3 SuperMini Hi-Fi USB DAC & DSP Engine");
    ESP_LOGI(TAG, " Hardware: PCM5102A + MAX97220 (BCK=10, DIN=11, WS=12)");
    ESP_LOGI(TAG, " Native USB: GPIO 19 (D-), GPIO 20 (D+)");
    ESP_LOGI(TAG, " Wi-Fi SoftAP Control: GPIO 0 (Boot Button)");
    ESP_LOGI(TAG, "==================================================");

    // 1. NVS Flash initialization
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1a. Hardware Pinout & Preferences Configuration
    hw_config_init();

    // 1b. LittleFS Flash Filesystem (Auto-partitioning & mounting on /storage)
    fs_manager_init();

    // 1c. OTA Firmware Manager (Dual-OTA rollback validation)
    ota_manager_init();

    // 2. Netif and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Preset Manager
    preset_manager_init();

    // 4. Audio Pipeline & DSP Engine (Core 1)
    ESP_ERROR_CHECK(audio_pipeline_init(48000, 16));

    // Load initial reference preset
    dsp_config_t default_cfg;
    if (preset_manager_load(0, &default_cfg) == ESP_OK) {
        dsp_engine_set_config(audio_pipeline_get_dsp_engine(), &default_cfg);
    }

    // 5. USB Audio Class Callback Handler
    usb_audio_init(on_sample_rate_changed);

    // 6. USB OTG & TinyUSB initialization
    init_usb_hardware();

    // 7. USB CDC Serial CLI
    serial_cli_init();

    // 8. Wi-Fi SoftAP & Captive Portal (Initial State: OFF)
    wifi_ap_init(on_wifi_state_changed);

    // 9. Boot button (GPIO 0) for Wi-Fi toggle
    boot_button_init();

    // 10. Built-in NeoPixel RGB Audio Visualizer (GPIO 48)
    neopixel_init();

    ESP_LOGI(TAG, "System ready. Connect USB to PC/Mac/Phone for bit-perfect audio streaming.");
    ESP_LOGI(TAG, "Press BOOT button (GPIO 0) at any time to open Wi-Fi Web UI (192.168.4.1).");
}
