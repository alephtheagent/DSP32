/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _PRESET_MANAGER_H_
#define _PRESET_MANAGER_H_

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "dsp/dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PRESETS 8
#define PRESET_NAME_MAX_LEN 32

typedef struct {
    char name[PRESET_NAME_MAX_LEN];
    dsp_config_t config;
    bool is_factory;
} preset_entry_t;

esp_err_t preset_manager_init(void);
size_t preset_manager_get_count(void);
const preset_entry_t *preset_manager_get_preset(size_t index);
esp_err_t preset_manager_load(size_t index, dsp_config_t *out_config);
esp_err_t preset_manager_save(size_t index, const char *name, const dsp_config_t *config);
esp_err_t preset_manager_reset_defaults(void);

#ifdef __cplusplus
}
#endif

#endif /* _PRESET_MANAGER_H_ */
