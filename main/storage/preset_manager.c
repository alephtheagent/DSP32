/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "preset_manager.h"
#include <string.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "PRESET_MGR";
#define NVS_NAMESPACE "dac_presets"

static preset_entry_t s_presets[MAX_PRESETS];

static void init_factory_presets(void)
{
    // Preset 0: Flat / Reference
    snprintf(s_presets[0].name, PRESET_NAME_MAX_LEN, "Reference Flat");
    memset(&s_presets[0].config, 0, sizeof(dsp_config_t));
    s_presets[0].config.limiter.enabled = true;
    s_presets[0].config.limiter.ceiling_db = -0.1f;
    s_presets[0].config.limiter.release_ms = 50.0f;
    s_presets[0].is_factory = true;

    // Preset 1: Warm Tube
    snprintf(s_presets[1].name, PRESET_NAME_MAX_LEN, "Warm Tube");
    memset(&s_presets[1].config, 0, sizeof(dsp_config_t));
    s_presets[1].config.tube.enabled = true;
    s_presets[1].config.tube.drive = 0.4f;
    s_presets[1].config.tube.warmth = 0.65f;
    s_presets[1].config.eq.enabled = true;
    s_presets[1].config.eq.bands[0].enabled = true;
    s_presets[1].config.eq.bands[0].gain_db = 2.0f; // low shelf +2dB
    s_presets[1].config.eq.bands[9].enabled = true;
    s_presets[1].config.eq.bands[9].gain_db = -1.2f; // smooth high roll
    s_presets[1].config.limiter.enabled = true;
    s_presets[1].config.limiter.ceiling_db = -0.2f;
    s_presets[1].is_factory = true;

    // Preset 2: Vocal Presence
    snprintf(s_presets[2].name, PRESET_NAME_MAX_LEN, "Vocal Presence");
    memset(&s_presets[2].config, 0, sizeof(dsp_config_t));
    s_presets[2].config.deesser.enabled = true;
    s_presets[2].config.deesser.freq = 6500.0f;
    s_presets[2].config.deesser.threshold_db = -26.0f;
    s_presets[2].config.deesser.ratio = 4.0f;
    s_presets[2].config.comp.enabled = true;
    s_presets[2].config.comp.threshold_db = -18.0f;
    s_presets[2].config.comp.ratio = 3.0f;
    s_presets[2].config.comp.attack_ms = 20.0f;
    s_presets[2].config.comp.release_ms = 80.0f;
    s_presets[2].config.eq.enabled = true;
    s_presets[2].config.eq.bands[6].enabled = true; // 2kHz
    s_presets[2].config.eq.bands[6].gain_db = 2.5f;
    s_presets[2].config.limiter.enabled = true;
    s_presets[2].config.limiter.ceiling_db = -0.2f;
    s_presets[2].is_factory = true;

    // Preset 3: Bass Booster
    snprintf(s_presets[3].name, PRESET_NAME_MAX_LEN, "Bass Booster");
    memset(&s_presets[3].config, 0, sizeof(dsp_config_t));
    s_presets[3].config.bass.enabled = true;
    s_presets[3].config.bass.cutoff_hz = 95.0f;
    s_presets[3].config.bass.drive = 0.6f;
    s_presets[3].config.bass.blend = 0.45f;
    s_presets[3].config.eq.enabled = true;
    s_presets[3].config.eq.bands[1].enabled = true; // 63Hz
    s_presets[3].config.eq.bands[1].gain_db = 3.5f;
    s_presets[3].config.limiter.enabled = true;
    s_presets[3].config.limiter.ceiling_db = -0.1f;
    s_presets[3].is_factory = true;

    // Preset 4: Binaural Crossfeed
    snprintf(s_presets[4].name, PRESET_NAME_MAX_LEN, "Binaural Crossfeed");
    memset(&s_presets[4].config, 0, sizeof(dsp_config_t));
    s_presets[4].config.crossfeed.enabled = true;
    s_presets[4].config.crossfeed.amount = 0.7f;
    s_presets[4].config.crossfeed.cutoff_hz = 700.0f;
    s_presets[4].config.widener.enabled = true;
    s_presets[4].config.widener.width = 1.15f;
    s_presets[4].config.limiter.enabled = true;
    s_presets[4].config.limiter.ceiling_db = -0.1f;
    s_presets[4].is_factory = true;

    // Preset 5: Night Mode Normalizer
    snprintf(s_presets[5].name, PRESET_NAME_MAX_LEN, "Night Normalizer");
    memset(&s_presets[5].config, 0, sizeof(dsp_config_t));
    s_presets[5].config.loudness.enabled = true;
    s_presets[5].config.loudness.target_db = -18.0f;
    s_presets[5].config.loudness.gate_db = -45.0f;
    s_presets[5].config.loudness.speed = 0.6f;
    s_presets[5].config.comp.enabled = true;
    s_presets[5].config.comp.threshold_db = -22.0f;
    s_presets[5].config.comp.ratio = 4.0f;
    s_presets[5].config.limiter.enabled = true;
    s_presets[5].config.limiter.ceiling_db = -1.0f;
    s_presets[5].is_factory = true;

    // Preset 6 & 7: User Presets
    snprintf(s_presets[6].name, PRESET_NAME_MAX_LEN, "User Preset 1");
    memset(&s_presets[6].config, 0, sizeof(dsp_config_t));
    s_presets[6].config.limiter.enabled = true;
    s_presets[6].config.limiter.ceiling_db = -0.1f;
    s_presets[6].is_factory = false;

    snprintf(s_presets[7].name, PRESET_NAME_MAX_LEN, "User Preset 2");
    memset(&s_presets[7].config, 0, sizeof(dsp_config_t));
    s_presets[7].config.limiter.enabled = true;
    s_presets[7].config.limiter.ceiling_db = -0.1f;
    s_presets[7].is_factory = false;
}

esp_err_t preset_manager_init(void)
{
    init_factory_presets();

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        for (size_t i = 0; i < MAX_PRESETS; i++) {
            char key[16];
            snprintf(key, sizeof(key), "p_%d", (int)i);
            size_t req_size = sizeof(preset_entry_t);
            preset_entry_t temp;
            if (nvs_get_blob(handle, key, &temp, &req_size) == ESP_OK) {
                s_presets[i] = temp;
            }
        }
        nvs_close(handle);
        ESP_LOGI(TAG, "Presets successfully loaded from NVS");
    } else {
        ESP_LOGI(TAG, "No saved presets found in NVS, initialized factory presets");
    }
    return ESP_OK;
}

size_t preset_manager_get_count(void)
{
    return MAX_PRESETS;
}

const preset_entry_t *preset_manager_get_preset(size_t index)
{
    if (index >= MAX_PRESETS) return NULL;
    return &s_presets[index];
}

esp_err_t preset_manager_load(size_t index, dsp_config_t *out_config)
{
    if (index >= MAX_PRESETS || !out_config) return ESP_ERR_INVALID_ARG;
    *out_config = s_presets[index].config;
    ESP_LOGI(TAG, "Loaded preset [%d]: %s", (int)index, s_presets[index].name);
    return ESP_OK;
}

esp_err_t preset_manager_save(size_t index, const char *name, const dsp_config_t *config)
{
    if (index >= MAX_PRESETS || !config) return ESP_ERR_INVALID_ARG;

    if (name && strlen(name) > 0) {
        snprintf(s_presets[index].name, PRESET_NAME_MAX_LEN, "%s", name);
    }
    s_presets[index].config = *config;
    s_presets[index].is_factory = false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        char key[16];
        snprintf(key, sizeof(key), "p_%d", (int)index);
        nvs_set_blob(handle, key, &s_presets[index], sizeof(preset_entry_t));
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Saved preset [%d] '%s' to NVS", (int)index, s_presets[index].name);
    }
    return err;
}

esp_err_t preset_manager_reset_defaults(void)
{
    init_factory_presets();
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
    ESP_LOGI(TAG, "Reset all presets to factory defaults");
    return ESP_OK;
}
