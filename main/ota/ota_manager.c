/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "ota_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage/fs_manager.h"

static const char *TAG = "OTA_MGR";
static FILE *s_cache_file = NULL;

esp_err_t ota_manager_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        ESP_LOGI(TAG, "Running firmware partition: %s (offset 0x%08" PRIx32 ", size 0x%08" PRIx32 ")",
                 running->label, running->address, running->size);
    }

    // Confirm that the running firmware is valid (prevents rollback)
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Current firmware confirmed valid, rollback cancelled");
    }

    // Ensure any leftover cache file from unexpected reboot is cleaned up
    struct stat st;
    if (stat(OTA_CACHE_FILE_PATH, &st) == 0) {
        unlink(OTA_CACHE_FILE_PATH);
        ESP_LOGI(TAG, "Cleaned up stale OTA cache file: %s", OTA_CACHE_FILE_PATH);
    }

    return ESP_OK;
}

esp_err_t ota_manager_get_status(ota_status_info_t *out_status)
{
    if (!out_status) return ESP_ERR_INVALID_ARG;
    memset(out_status, 0, sizeof(ota_status_info_t));

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        strncpy(out_status->running_partition, running->label, sizeof(out_status->running_partition) - 1);
    } else {
        strncpy(out_status->running_partition, "unknown", sizeof(out_status->running_partition) - 1);
    }

    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (target) {
        strncpy(out_status->target_partition, target->label, sizeof(out_status->target_partition) - 1);
    } else {
        strncpy(out_status->target_partition, "none", sizeof(out_status->target_partition) - 1);
    }

    const esp_app_desc_t *app_desc = esp_app_get_description();
    if (app_desc) {
        strncpy(out_status->version, app_desc->version, sizeof(out_status->version) - 1);
        strncpy(out_status->project_name, app_desc->project_name, sizeof(out_status->project_name) - 1);
        strncpy(out_status->compile_date, app_desc->date, sizeof(out_status->compile_date) - 1);
        strncpy(out_status->compile_time, app_desc->time, sizeof(out_status->compile_time) - 1);
        strncpy(out_status->idf_version, app_desc->idf_ver, sizeof(out_status->idf_version) - 1);
    }

    size_t total = 0, used = 0;
    if (fs_manager_get_info(&total, &used) == ESP_OK) {
        out_status->littlefs_total_bytes = total;
        out_status->littlefs_free_bytes = (total >= used) ? (total - used) : 0;
    }

    return ESP_OK;
}

esp_err_t ota_manager_start_cache(void)
{
    ota_manager_abort();

    s_cache_file = fopen(OTA_CACHE_FILE_PATH, "wb");
    if (!s_cache_file) {
        ESP_LOGE(TAG, "Failed to open cache file '%s' for writing", OTA_CACHE_FILE_PATH);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Started caching OTA firmware to '%s'", OTA_CACHE_FILE_PATH);
    return ESP_OK;
}

esp_err_t ota_manager_write_cache(const void *data, size_t length)
{
    if (!s_cache_file || !data || length == 0) return ESP_ERR_INVALID_STATE;

    size_t written = fwrite(data, 1, length, s_cache_file);
    if (written != length) {
        ESP_LOGE(TAG, "Disk write error: requested %u bytes, wrote %u bytes", (unsigned int)length, (unsigned int)written);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void ota_manager_abort(void)
{
    if (s_cache_file) {
        fclose(s_cache_file);
        s_cache_file = NULL;
    }
    struct stat st;
    if (stat(OTA_CACHE_FILE_PATH, &st) == 0) {
        unlink(OTA_CACHE_FILE_PATH);
    }
}

static void restart_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Rebooting in 1.5 seconds into updated firmware...");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

esp_err_t ota_manager_finish_cache_and_flash(char *err_msg, size_t err_msg_sz)
{
    if (s_cache_file) {
        fclose(s_cache_file);
        s_cache_file = NULL;
    }

    FILE *f = fopen(OTA_CACHE_FILE_PATH, "rb");
    if (!f) {
        if (err_msg) snprintf(err_msg, err_msg_sz, "Cannot open cached firmware file");
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    ESP_LOGI(TAG, "Verifying cached firmware file (size: %ld bytes)...", file_size);

    if (file_size < (64 * 1024)) {
        fclose(f);
        unlink(OTA_CACHE_FILE_PATH);
        if (err_msg) snprintf(err_msg, err_msg_sz, "Firmware file too small (%ld bytes)", file_size);
        return ESP_ERR_INVALID_SIZE;
    }

    // Read and validate ESP image header
    esp_image_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        unlink(OTA_CACHE_FILE_PATH);
        if (err_msg) snprintf(err_msg, err_msg_sz, "Failed to read image header");
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (header.magic != ESP_IMAGE_HEADER_MAGIC) {
        fclose(f);
        unlink(OTA_CACHE_FILE_PATH);
        ESP_LOGE(TAG, "Header magic mismatch: expected 0x%02X, got 0x%02X", ESP_IMAGE_HEADER_MAGIC, header.magic);
        if (err_msg) snprintf(err_msg, err_msg_sz, "Invalid file format: not an ESP32 application image");
        return ESP_ERR_INVALID_VERSION;
    }

    if (header.chip_id != ESP_CHIP_ID_ESP32S3) {
        fclose(f);
        unlink(OTA_CACHE_FILE_PATH);
        ESP_LOGE(TAG, "Chip ID mismatch: expected ESP32-S3 (0x0009), got 0x%04X", header.chip_id);
        if (err_msg) snprintf(err_msg, err_msg_sz, "Binary is not built for ESP32-S3 chip");
        return ESP_ERR_INVALID_VERSION;
    }

    rewind(f);

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        fclose(f);
        unlink(OTA_CACHE_FILE_PATH);
        if (err_msg) snprintf(err_msg, err_msg_sz, "No available OTA update partition found");
        return ESP_ERR_NOT_FOUND;
    }

    if ((size_t)file_size > update_partition->size) {
        fclose(f);
        unlink(OTA_CACHE_FILE_PATH);
        if (err_msg) snprintf(err_msg, err_msg_sz, "Firmware size (%ld bytes) exceeds partition capacity (%u bytes)",
                              file_size, (unsigned int)update_partition->size);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Writing firmware to target partition '%s' (0x%08" PRIx32 ", size %u KB)...",
             update_partition->label, update_partition->address, (unsigned int)(update_partition->size / 1024));

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, (size_t)file_size, &ota_handle);
    if (err != ESP_OK) {
        fclose(f);
        unlink(OTA_CACHE_FILE_PATH);
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        if (err_msg) snprintf(err_msg, err_msg_sz, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    char chunk[4096];
    size_t written_total = 0;
    while (!feof(f)) {
        size_t bytes_read = fread(chunk, 1, sizeof(chunk), f);
        if (bytes_read > 0) {
            err = esp_ota_write(ota_handle, chunk, bytes_read);
            if (err != ESP_OK) {
                esp_ota_abort(ota_handle);
                fclose(f);
                unlink(OTA_CACHE_FILE_PATH);
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                if (err_msg) snprintf(err_msg, err_msg_sz, "Flash write failed: %s", esp_err_to_name(err));
                return err;
            }
            written_total += bytes_read;
        }
    }

    fclose(f);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        unlink(OTA_CACHE_FILE_PATH);
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        if (err_msg) snprintf(err_msg, err_msg_sz, "esp_ota_end validation failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        unlink(OTA_CACHE_FILE_PATH);
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        if (err_msg) snprintf(err_msg, err_msg_sz, "Failed to set boot partition: %s", esp_err_to_name(err));
        return err;
    }

    // Delete cached file to release LittleFS space
    unlink(OTA_CACHE_FILE_PATH);
    ESP_LOGI(TAG, "Firmware successfully flashed to '%s'. Cache file deleted.", update_partition->label);

    // Schedule reboot
    xTaskCreate(restart_task, "ota_reboot", 2048, NULL, 5, NULL);

    return ESP_OK;
}
