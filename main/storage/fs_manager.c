/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "fs_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_log.h"
#include "esp_littlefs.h"

static const char *TAG = "FS_MGR";
static bool s_mounted = false;

static void create_default_structure(void)
{
    struct stat st;
    if (stat(FS_STORAGE_MOUNT_POINT "/presets", &st) != 0) {
        mkdir(FS_STORAGE_MOUNT_POINT "/presets", 0777);
        ESP_LOGI(TAG, "Created directory: " FS_STORAGE_MOUNT_POINT "/presets");
    }
    if (stat(FS_STORAGE_MOUNT_POINT "/audio", &st) != 0) {
        mkdir(FS_STORAGE_MOUNT_POINT "/audio", 0777);
        ESP_LOGI(TAG, "Created directory: " FS_STORAGE_MOUNT_POINT "/audio");
    }

    if (stat(FS_STORAGE_MOUNT_POINT "/README.txt", &st) != 0) {
        FILE *f = fopen(FS_STORAGE_MOUNT_POINT "/README.txt", "w");
        if (f) {
            size_t total = 0, used = 0;
            esp_littlefs_info(FS_STORAGE_PARTITION_LABEL, &total, &used);
            fprintf(f, "ESP32-S3 SuperMini Hi-Fi USB DAC & DSP Engine\n");
            fprintf(f, "Filesystem: LittleFS (Fail-Safe Microcontroller FS)\n");
            fprintf(f, "Mount Point: " FS_STORAGE_MOUNT_POINT "\n");
            fprintf(f, "Total Capacity: %u KB (%.2f MB)\n", (unsigned int)(total / 1024), (double)total / (1024.0 * 1024.0));
            fprintf(f, "Purpose: Presets, Impulse Responses (IR), and Config storage.\n");
            fclose(f);
            ESP_LOGI(TAG, "Initialized default marker: " FS_STORAGE_MOUNT_POINT "/README.txt");
        }
    }
}

esp_err_t fs_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing LittleFS storage on partition '%s'...", FS_STORAGE_PARTITION_LABEL);

    esp_vfs_littlefs_conf_t conf = {
        .base_path = FS_STORAGE_MOUNT_POINT,
        .partition_label = FS_STORAGE_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
        .grow_on_mount = true,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format LittleFS filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition '%s'", FS_STORAGE_PARTITION_LABEL);
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        s_mounted = false;
        return ret;
    }

    s_mounted = true;

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(FS_STORAGE_PARTITION_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted successfully: Total: %u KB, Used: %u KB, Free: %u KB (%.1f%% used)",
                 (unsigned int)(total / 1024),
                 (unsigned int)(used / 1024),
                 (unsigned int)((total - used) / 1024),
                 total > 0 ? ((double)used / (double)total * 100.0) : 0.0);
    } else {
        ESP_LOGW(TAG, "LittleFS mounted, but failed to retrieve partition stats (%s)", esp_err_to_name(ret));
    }

    create_default_structure();

    return ESP_OK;
}

bool fs_manager_is_mounted(void)
{
    return s_mounted;
}

esp_err_t fs_manager_get_info(size_t *total_bytes, size_t *used_bytes)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    return esp_littlefs_info(FS_STORAGE_PARTITION_LABEL, total_bytes, used_bytes);
}

esp_err_t fs_manager_format(void)
{
    if (s_mounted) {
        esp_vfs_littlefs_unregister(FS_STORAGE_PARTITION_LABEL);
        s_mounted = false;
    }

    ESP_LOGI(TAG, "Formatting LittleFS partition '%s'...", FS_STORAGE_PARTITION_LABEL);
    esp_err_t ret = esp_littlefs_format(FS_STORAGE_PARTITION_LABEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format LittleFS partition (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Format complete. Re-mounting...");
    return fs_manager_init();
}
