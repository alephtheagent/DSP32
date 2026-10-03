/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "serial_cli.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "esp_log.h"
#include "audio_pipeline.h"
#include "usb_audio.h"
#include "wifi/wifi_ap.h"
#include "storage/preset_manager.h"
#include "storage/fs_manager.h"
#include "storage/hw_config.h"
#include "esp_system.h"
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "CLI";
#define CLI_LINE_BUF_SZ 128

#if CFG_TUD_CDC
static void cdc_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0 && tud_cdc_connected()) {
        tud_cdc_write(buf, (uint32_t)len);
        tud_cdc_write_flush();
    }
    printf("%s", buf);
}

static void print_help(void)
{
    cdc_printf("\r\n=== DSP32-S3 USB DAC Serial CLI Commands ===\r\n");
    cdc_printf("  help                         - Show this help menu\r\n");
    cdc_printf("  status                       - Show current audio, buffer and Wi-Fi status\r\n");
    cdc_printf("  bypass [on|off]              - Get or toggle Bit-Perfect Direct mode\r\n");
    cdc_printf("  buffer <16|32|64|128|256>    - Set DMA buffer latency in samples\r\n");
    cdc_printf("  vol <dB>                     - Set master volume (-60.0 to 0.0 dB)\r\n");
    cdc_printf("  eq <band 0-9> <gain_db>      - Set EQ band gain\r\n");
    cdc_printf("  fx <name> <on|off>           - Toggle FX (comp, lim, tube, cross, wide, bass, deess, pitch, loud, fx)\r\n");
    cdc_printf("  preset <list|load <n>|reset> - Manage presets\r\n");
    cdc_printf("  storage <info|ls|cat|format> - LittleFS flash storage manager\r\n");
    cdc_printf("  pins [set <bck> <din> <ws>|reset] - Manage hardware pinout\r\n");
    cdc_printf("  wifi <on|off|toggle|status>  - Control Wi-Fi SoftAP\r\n");
    cdc_printf("  vu                           - Print instant VU meter readings\r\n");
    cdc_printf("============================================\r\n\r\n");
}

static void handle_command(char *line)
{
    // Trim leading whitespace
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    char *cmd = strtok(line, " \t\r\n");
    if (!cmd) return;

    dsp_engine_t *engine = audio_pipeline_get_dsp_engine();

    if (strcmp(cmd, "help") == 0) {
        print_help();
    } else if (strcmp(cmd, "status") == 0) {
        audio_pipeline_stats_t stats;
        audio_pipeline_get_stats(&stats);
        usb_audio_status_t u_stats;
        usb_audio_get_status(&u_stats);

        cdc_printf("\r\n--- Status Report ---\r\n");
        cdc_printf("Sample Rate : %" PRIu32 " Hz\r\n", stats.sample_rate);
        cdc_printf("Bit Depth   : %d-bit PCM (%s)\r\n", stats.bit_depth, stats.bit_depth == 24 ? "in 32-bit slot" : "standard");
        cdc_printf("Bit-Perfect : %s\r\n", stats.bit_perfect ? "ACTIVE (Direct DAC)" : "OFF (DSP Chain Active)");
        cdc_printf("Buffer Size : %d samples (~%.1f ms)\r\n", (int)stats.buffer_size_samples, (float)stats.buffer_size_samples / (stats.sample_rate / 1000.0f));
        cdc_printf("Streaming   : %s\r\n", u_stats.is_streaming ? "YES (Active)" : "NO (Idle)");
        cdc_printf("Underruns   : %" PRIu32 " | Overruns: %" PRIu32 "\r\n", stats.underrun_count, stats.overrun_count);
        cdc_printf("Wi-Fi SoftAP: %s (Stations: %d)\r\n", wifi_ap_is_active() ? "ON (192.168.4.1)" : "OFF (Zero RF Noise)", wifi_ap_get_station_count());
        cdc_printf("---------------------\r\n");
    } else if (strcmp(cmd, "bypass") == 0) {
        char *arg = strtok(NULL, " \t\r\n");
        if (arg) {
            bool on = (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0);
            dsp_engine_set_bit_perfect(engine, on);
        }
        cdc_printf("Bit-Perfect Bypass: %s\r\n", dsp_engine_is_bit_perfect(engine) ? "ON (DIRECT)" : "OFF");
    } else if (strcmp(cmd, "buffer") == 0) {
        char *arg = strtok(NULL, " \t\r\n");
        if (arg) {
            int sz = atoi(arg);
            audio_pipeline_set_buffer_size((audio_buffer_size_t)sz);
            cdc_printf("Buffer size set to %d samples\r\n", (int)audio_pipeline_get_buffer_size());
        } else {
            cdc_printf("Current buffer size: %d samples\r\n", (int)audio_pipeline_get_buffer_size());
        }
    } else if (strcmp(cmd, "vol") == 0) {
        char *arg = strtok(NULL, " \t\r\n");
        if (arg) {
            float v = atof(arg);
            dsp_engine_set_master_volume(engine, v);
            cdc_printf("Master volume set to %.1f dB\r\n", v);
        }
    } else if (strcmp(cmd, "eq") == 0) {
        char *b_str = strtok(NULL, " \t\r\n");
        char *g_str = strtok(NULL, " \t\r\n");
        if (b_str && g_str) {
            int b = atoi(b_str);
            float g = atof(g_str);
            if (b >= 0 && b < DSP_EQ_NUM_BANDS) {
                dsp_eq_set_band(&engine->eq, b, g, 0.0f, 0.0f, false);
                cdc_printf("EQ Band %d gain set to %.1f dB\r\n", b, g);
            }
        }
    } else if (strcmp(cmd, "wifi") == 0) {
        char *arg = strtok(NULL, " \t\r\n");
        if (arg) {
            if (strcmp(arg, "on") == 0) wifi_ap_start();
            else if (strcmp(arg, "off") == 0) wifi_ap_stop();
            else if (strcmp(arg, "toggle") == 0) wifi_ap_toggle();
        }
        cdc_printf("Wi-Fi SoftAP: %s\r\n", wifi_ap_is_active() ? "ON" : "OFF");
    } else if (strcmp(cmd, "preset") == 0) {
        char *sub = strtok(NULL, " \t\r\n");
        if (sub && strcmp(sub, "list") == 0) {
            size_t cnt = preset_manager_get_count();
            for (size_t i = 0; i < cnt; i++) {
                const preset_entry_t *p = preset_manager_get_preset(i);
                if (p) cdc_printf("[%d] %s (%s)\r\n", (int)i, p->name, p->is_factory ? "Factory" : "User");
            }
        } else if (sub && strcmp(sub, "load") == 0) {
            char *idx_str = strtok(NULL, " \t\r\n");
            if (idx_str) {
                int idx = atoi(idx_str);
                dsp_config_t cfg;
                if (preset_manager_load(idx, &cfg) == ESP_OK) {
                    dsp_engine_set_config(engine, &cfg);
                    cdc_printf("Loaded preset [%d]\r\n", idx);
                }
            }
        } else if (sub && strcmp(sub, "reset") == 0) {
            preset_manager_reset_defaults();
            cdc_printf("Presets reset to factory defaults\r\n");
        }
    } else if (strcmp(cmd, "vu") == 0) {
        dsp_meter_values_t m;
        dsp_engine_get_meters(engine, &m);
        cdc_printf("In L: %.1f dB | In R: %.1f dB || Out L: %.1f dB | Out R: %.1f dB\r\n",
                   m.in_peak_l, m.in_peak_r, m.out_peak_l, m.out_peak_r);
    } else if (strcmp(cmd, "storage") == 0) {
        char *sub = strtok(NULL, " \t\r\n");
        if (!sub || strcmp(sub, "info") == 0) {
            size_t total = 0, used = 0;
            if (fs_manager_get_info(&total, &used) == ESP_OK) {
                cdc_printf("\r\n--- LittleFS Flash Storage ---\r\n");
                cdc_printf("Status      : Mounted at " FS_STORAGE_MOUNT_POINT "\r\n");
                cdc_printf("Total Space : %u bytes (~%u KB / %.2f MB)\r\n", (unsigned int)total, (unsigned int)(total / 1024), (double)total / (1024.0 * 1024.0));
                cdc_printf("Used Space  : %u bytes (~%u KB)\r\n", (unsigned int)used, (unsigned int)(used / 1024));
                cdc_printf("Free Space  : %u bytes (~%u KB / %.2f MB)\r\n", (unsigned int)(total - used), (unsigned int)((total - used) / 1024), (double)(total - used) / (1024.0 * 1024.0));
                cdc_printf("Utilization : %.1f%%\r\n", total > 0 ? ((double)used / (double)total * 100.0) : 0.0);
                cdc_printf("------------------------------\r\n");
            } else {
                cdc_printf("LittleFS not mounted or error reading partition info.\r\n");
            }
        } else if (strcmp(sub, "ls") == 0) {
            char *dir_arg = strtok(NULL, " \t\r\n");
            const char *path = dir_arg ? dir_arg : FS_STORAGE_MOUNT_POINT;
            DIR *d = opendir(path);
            if (!d) {
                cdc_printf("Failed to open directory '%s'\r\n", path);
            } else {
                cdc_printf("\r\nDirectory listing of '%s':\r\n", path);
                struct dirent *de;
                int count = 0;
                while ((de = readdir(d)) != NULL) {
                    char fullpath[128];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, de->d_name);
                    struct stat st;
                    if (stat(fullpath, &st) == 0) {
                        if (S_ISDIR(st.st_mode)) {
                            cdc_printf("  <DIR>  %-24s\r\n", de->d_name);
                        } else {
                            cdc_printf("  %6ld %-24s\r\n", (long)st.st_size, de->d_name);
                        }
                    } else {
                        cdc_printf("  ?????? %-24s\r\n", de->d_name);
                    }
                    count++;
                }
                closedir(d);
                cdc_printf("Total %d entries.\r\n", count);
            }
        } else if (strcmp(sub, "cat") == 0) {
            char *fpath = strtok(NULL, " \t\r\n");
            if (!fpath) {
                cdc_printf("Usage: storage cat <file_path>\r\n");
            } else {
                FILE *f = fopen(fpath, "r");
                if (!f) {
                    cdc_printf("Cannot open '%s'\r\n", fpath);
                } else {
                    cdc_printf("\r\n--- Begin of '%s' ---\r\n", fpath);
                    char readbuf[64];
                    size_t nr;
                    while ((nr = fread(readbuf, 1, sizeof(readbuf) - 1, f)) > 0) {
                        readbuf[nr] = '\0';
                        cdc_printf("%s", readbuf);
                    }
                    fclose(f);
                    cdc_printf("\r\n--- End of file ---\r\n");
                }
            }
        } else if (strcmp(sub, "format") == 0) {
            cdc_printf("Formatting LittleFS partition... ");
            if (fs_manager_format() == ESP_OK) {
                cdc_printf("SUCCESS. Re-mounted at " FS_STORAGE_MOUNT_POINT "\r\n");
            } else {
                cdc_printf("FAILED.\r\n");
            }
        } else {
            cdc_printf("Usage: storage [info|ls [path]|cat <file>|format]\r\n");
        }
    } else if (strcmp(cmd, "pins") == 0) {
        char *sub = strtok(NULL, " \t\r\n");
        if (!sub) {
            const hw_config_t *cfg = hw_config_get();
            cdc_printf("Current Hardware Pinout:\r\n");
            cdc_printf("  I2S BCK : GPIO %d\r\n", cfg->i2s_bck_gpio);
            cdc_printf("  I2S DIN : GPIO %d\r\n", cfg->i2s_din_gpio);
            cdc_printf("  I2S WS  : GPIO %d\r\n", cfg->i2s_ws_gpio);
            cdc_printf("  NeoPixel: GPIO %d\r\n", cfg->neopixel_gpio);
            cdc_printf("  BOOT Btn: GPIO %d\r\n", cfg->boot_button_gpio);
            cdc_printf("  Wi-Fi   : SSID '%s' (Ch %d)\r\n", cfg->wifi_ssid, cfg->wifi_channel);
        } else if (strcmp(sub, "set") == 0) {
            char *b_str = strtok(NULL, " \t\r\n");
            char *d_str = strtok(NULL, " \t\r\n");
            char *w_str = strtok(NULL, " \t\r\n");
            if (b_str && d_str && w_str) {
                hw_config_t cfg = *hw_config_get();
                cfg.i2s_bck_gpio = atoi(b_str);
                cfg.i2s_din_gpio = atoi(d_str);
                cfg.i2s_ws_gpio = atoi(w_str);
                hw_config_set(&cfg);
                cdc_printf("Pins saved (BCK=%d, DIN=%d, WS=%d). Rebooting to apply...\r\n",
                           cfg.i2s_bck_gpio, cfg.i2s_din_gpio, cfg.i2s_ws_gpio);
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
            } else {
                cdc_printf("Usage: pins set <bck> <din> <ws>\r\n");
            }
        } else if (strcmp(sub, "reset") == 0) {
            hw_config_reset_defaults();
            cdc_printf("Hardware config reset to defaults. Rebooting...\r\n");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        } else {
            cdc_printf("Usage: pins [set <bck> <din> <ws>|reset]\r\n");
        }
    } else {
        cdc_printf("Unknown command '%s'. Type 'help' for manual.\r\n", cmd);
    }

    cdc_printf("DSP32> ");
}

static void serial_cli_task(void *pvParameters)
{
    char line_buf[CLI_LINE_BUF_SZ];
    size_t line_pos = 0;

    while (1) {
        if (tud_cdc_available()) {
            uint8_t ch = 0;
            if (tud_cdc_read(&ch, 1) == 1) {
                // Echo back
                tud_cdc_write(&ch, 1);
                tud_cdc_write_flush();

                if (ch == '\r' || ch == '\n') {
                    cdc_printf("\r\n");
                    line_buf[line_pos] = '\0';
                    if (line_pos > 0) {
                        handle_command(line_buf);
                        line_pos = 0;
                    } else {
                        cdc_printf("DSP32> ");
                    }
                } else if (ch == '\b' || ch == 127) {
                    if (line_pos > 0) {
                        line_pos--;
                        cdc_printf(" \b");
                    }
                } else if (line_pos < CLI_LINE_BUF_SZ - 1) {
                    line_buf[line_pos++] = (char)ch;
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}
#endif

esp_err_t serial_cli_init(void)
{
#if CFG_TUD_CDC
    BaseType_t ret = xTaskCreatePinnedToCore(serial_cli_task, "serial_cli", 4096, NULL, 4, NULL, 0);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CLI task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Serial CLI initialized on USB CDC port");
#else
    ESP_LOGI(TAG, "Serial CLI over USB CDC disabled (Pure Audio mode)");
#endif
    return ESP_OK;
}
