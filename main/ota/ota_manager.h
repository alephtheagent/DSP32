/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _OTA_MANAGER_H_
#define _OTA_MANAGER_H_

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_CACHE_FILE_PATH "/storage/update.bin"

typedef struct {
    char running_partition[16];
    char target_partition[16];
    char version[32];
    char project_name[32];
    char compile_date[16];
    char compile_time[16];
    char idf_version[32];
    size_t littlefs_free_bytes;
    size_t littlefs_total_bytes;
} ota_status_info_t;

esp_err_t ota_manager_init(void);
esp_err_t ota_manager_get_status(ota_status_info_t *out_status);

// Caching and flashing workflow
esp_err_t ota_manager_start_cache(void);
esp_err_t ota_manager_write_cache(const void *data, size_t length);
esp_err_t ota_manager_finish_cache_and_flash(char *err_msg, size_t err_msg_sz);
void ota_manager_abort(void);

#ifdef __cplusplus
}
#endif

#endif /* _OTA_MANAGER_H_ */
