/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _FS_MANAGER_H_
#define _FS_MANAGER_H_

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FS_STORAGE_MOUNT_POINT "/storage"
#define FS_STORAGE_PARTITION_LABEL "storage"

esp_err_t fs_manager_init(void);
bool fs_manager_is_mounted(void);
esp_err_t fs_manager_get_info(size_t *total_bytes, size_t *used_bytes);
esp_err_t fs_manager_format(void);

#ifdef __cplusplus
}
#endif

#endif /* _FS_MANAGER_H_ */
