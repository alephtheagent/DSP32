/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "neopixel.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "audio_pipeline.h"
#include "usb_audio.h"
#include "wifi/wifi_ap.h"
#include "storage/hw_config.h"

static const char *TAG = "NEOPIXEL";

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution (0.1us tick)
#define NVS_NAMESPACE               "dac_presets"
#define NVS_KEY_LED                 "led_cfg"

static rmt_channel_handle_t s_led_chan = NULL;
static rmt_encoder_handle_t s_encoder = NULL;
static neopixel_config_t    s_config = {
    .mode = NEOPIXEL_MODE_VU_PEAK,
    .brightness = 50
};

// WS2812 timing symbols (at 10 MHz: 1 tick = 0.1us)
static const rmt_symbol_word_t ws2812_zero = {
    .level0 = 1,
    .duration0 = 3,  // 0.35 us T0H
    .level1 = 0,
    .duration1 = 9,  // 0.90 us T0L
};

static const rmt_symbol_word_t ws2812_one = {
    .level0 = 1,
    .duration0 = 9,  // 0.90 us T1H
    .level1 = 0,
    .duration1 = 3,  // 0.35 us T1L
};

static const rmt_symbol_word_t ws2812_reset = {
    .level0 = 0,
    .duration0 = 250, // 25 us
    .level1 = 0,
    .duration1 = 250, // 25 us (total > 50us reset)
};

static size_t encoder_callback(const void *data, size_t data_size,
                               size_t symbols_written, size_t symbols_free,
                               rmt_symbol_word_t *symbols, bool *done, void *arg)
{
    if (symbols_free < 8) return 0;

    size_t data_pos = symbols_written / 8;
    uint8_t *bytes = (uint8_t *)data;

    if (data_pos < data_size) {
        size_t symbol_pos = 0;
        for (int mask = 0x80; mask != 0; mask >>= 1) {
            symbols[symbol_pos++] = (bytes[data_pos] & mask) ? ws2812_one : ws2812_zero;
        }
        return symbol_pos;
    } else {
        symbols[0] = ws2812_reset;
        *done = true;
        return 1;
    }
}

static void send_grb(uint8_t g, uint8_t r, uint8_t b)
{
    if (!s_led_chan || !s_encoder) return;

    // Apply global brightness scale (0..100)
    uint32_t br = s_config.brightness;
    uint8_t scaled_g = (uint8_t)((g * br) / 100);
    uint8_t scaled_r = (uint8_t)((r * br) / 100);
    uint8_t scaled_b = (uint8_t)((b * br) / 100);

    uint8_t grb[3] = { scaled_g, scaled_r, scaled_b };

    rmt_transmit_config_t tx_config = { .loop_count = 0 };
    rmt_transmit(s_led_chan, s_encoder, grb, sizeof(grb), &tx_config);
    rmt_tx_wait_all_done(s_led_chan, 10);
}

// Convert HSV (hue 0..360, sat 0..1, val 0..1) to RGB (0..255)
static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float c = v * s;
    float h_prime = fmodf(h / 60.0f, 6.0f);
    float x = c * (1.0f - fabsf(fmodf(h_prime, 2.0f) - 1.0f));
    float m = v - c;

    float r1 = 0, g1 = 0, b1 = 0;
    if (0 <= h_prime && h_prime < 1)      { r1 = c; g1 = x; b1 = 0; }
    else if (1 <= h_prime && h_prime < 2) { r1 = x; g1 = c; b1 = 0; }
    else if (2 <= h_prime && h_prime < 3) { r1 = 0; g1 = c; b1 = x; }
    else if (3 <= h_prime && h_prime < 4) { r1 = 0; g1 = x; b1 = c; }
    else if (4 <= h_prime && h_prime < 5) { r1 = x; g1 = 0; b1 = c; }
    else if (5 <= h_prime && h_prime < 6) { r1 = c; g1 = 0; b1 = x; }

    *r = (uint8_t)((r1 + m) * 255.0f);
    *g = (uint8_t)((g1 + m) * 255.0f);
    *b = (uint8_t)((b1 + m) * 255.0f);
}

static void save_nvs_config(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, NVS_KEY_LED, &s_config, sizeof(s_config));
        nvs_commit(h);
        nvs_close(h);
    }
}

static void load_nvs_config(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof(s_config);
        nvs_get_blob(h, NVS_KEY_LED, &s_config, &sz);
        nvs_close(h);
    }
}

static void neopixel_task(void *pvParameters)
{
    ESP_LOGI(TAG, "NeoPixel Visualizer Task started on Core %d (GPIO %d)", xPortGetCoreID(), NEOPIXEL_GPIO_NUM);

    float rainbow_hue = 0.0f;
    float smooth_peak = 0.0f;
    float smooth_rms = 0.0f;
    float status_phase = 0.0f;

    while (1) {
        if (s_config.mode == NEOPIXEL_MODE_OFF || s_config.brightness == 0) {
            send_grb(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        dsp_engine_t *engine = audio_pipeline_get_dsp_engine();
        dsp_meter_values_t m;
        dsp_engine_get_meters(engine, &m);

        usb_audio_status_t u_stats;
        usb_audio_get_status(&u_stats);

        float peak_db = fmaxf(m.out_peak_l, m.out_peak_r);
        float rms_db = fmaxf(m.out_rms_l, m.out_rms_r);

        // Normalize dB (-50 dB to 0 dB -> 0.0 to 1.0)
        float cur_peak = (peak_db > -50.0f) ? ((peak_db + 50.0f) / 50.0f) : 0.0f;
        if (cur_peak > 1.0f) cur_peak = 1.0f;

        float cur_rms = (rms_db > -50.0f) ? ((rms_db + 50.0f) / 50.0f) : 0.0f;
        if (cur_rms > 1.0f) cur_rms = 1.0f;

        // Ballistics: fast attack, smooth decay
        if (cur_peak > smooth_peak) smooth_peak = cur_peak;
        else smooth_peak = smooth_peak * 0.85f + cur_peak * 0.15f;

        if (cur_rms > smooth_rms) smooth_rms = cur_rms;
        else smooth_rms = smooth_rms * 0.88f + cur_rms * 0.12f;

        uint8_t r = 0, g = 0, b = 0;

        switch (s_config.mode) {
        case NEOPIXEL_MODE_VU_PEAK: {
            if (smooth_peak < 0.02f) {
                // Dim gentle idle green
                g = 15; r = 0; b = 0;
            } else if (smooth_peak < 0.60f) {
                // Green zone (-50 dB .. -20 dB)
                float t = smooth_peak / 0.60f;
                g = (uint8_t)(80 + 175 * t);
                r = (uint8_t)(40 * t);
                b = 0;
            } else if (smooth_peak < 0.85f) {
                // Yellow / Amber zone (-20 dB .. -7 dB)
                float t = (smooth_peak - 0.60f) / 0.25f;
                r = 255;
                g = (uint8_t)(220 - 140 * t);
                b = 0;
            } else {
                // Peak Red zone (-7 dB .. 0 dB)
                float t = (smooth_peak - 0.85f) / 0.15f;
                r = 255;
                g = (uint8_t)(80 * (1.0f - t));
                b = (uint8_t)(40 * t);
            }
            break;
        }

        case NEOPIXEL_MODE_BASS_PULSE: {
            // Neon Violet / Deep Blue baseline, flashes Electric Cyan / Magenta on beat
            float intensity = smooth_peak;
            r = (uint8_t)(70.0f * (1.0f - intensity) + 255.0f * (intensity * intensity));
            g = (uint8_t)(220.0f * intensity);
            b = (uint8_t)(255.0f * fmaxf(0.3f, intensity));
            break;
        }

        case NEOPIXEL_MODE_RAINBOW: {
            // Spectrum rotation modulated by RMS energy
            rainbow_hue += 3.0f;
            if (rainbow_hue >= 360.0f) rainbow_hue -= 360.0f;

            float val = 0.20f + 0.80f * smooth_rms;
            hsv_to_rgb(rainbow_hue, 1.0f, val, &r, &g, &b);
            break;
        }

        case NEOPIXEL_MODE_TUBE_FIRE: {
            // Warm vintage analog tube filament glow with organic flicker
            float flicker = ((float)(rand() % 20) - 10.0f) / 250.0f;
            float heat = fminf(1.0f, fmaxf(0.0f, smooth_rms + flicker));
            r = 255;
            g = (uint8_t)(45.0f + 130.0f * heat);
            b = (uint8_t)(10.0f * heat);
            break;
        }

        case NEOPIXEL_MODE_STATUS: {
            status_phase += 0.08f;
            if (status_phase > 2.0f * (float)M_PI) status_phase -= 2.0f * (float)M_PI;
            float breath = 0.5f + 0.5f * sinf(status_phase);

            if (u_stats.is_streaming) {
                // Cyan breathing when active streaming
                r = 0;
                g = (uint8_t)(100 + 155 * breath);
                b = (uint8_t)(140 + 115 * breath);
            } else if (wifi_ap_is_active()) {
                // Amber breathing when Wi-Fi SoftAP active
                r = (uint8_t)(160 + 95 * breath);
                g = (uint8_t)(80 + 60 * breath);
                b = 0;
            } else {
                // Soft deep blue when ready / idle
                r = 0;
                g = (uint8_t)(20 * breath);
                b = (uint8_t)(60 + 80 * breath);
            }
            break;
        }

        default:
            break;
        }

        send_grb(g, r, b);
        vTaskDelay(pdMS_TO_TICKS(30)); // ~33 FPS
    }
}

esp_err_t neopixel_init(void)
{
    load_nvs_config();

    const hw_config_t *hw = hw_config_get();
    int gpio_num = (hw && hw->neopixel_gpio >= 0) ? hw->neopixel_gpio : NEOPIXEL_GPIO_NUM;
    if (gpio_num < 0) {
        ESP_LOGI(TAG, "NeoPixel disabled in hardware configuration");
        return ESP_OK;
    }

    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = (gpio_num_t)gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &s_led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel on GPIO %d: %s", gpio_num, esp_err_to_name(ret));
        return ret;
    }

    const rmt_simple_encoder_config_t simple_encoder_cfg = {
        .callback = encoder_callback
    };
    ret = rmt_new_simple_encoder(&simple_encoder_cfg, &s_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT simple encoder: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(rmt_enable(s_led_chan));

    // Clear LED initially (turn off immediately)
    send_grb(0, 0, 0);

    // Create background visualizer task on Core 0 (low priority 2)
    xTaskCreatePinnedToCore(neopixel_task, "neopixel_fx", 3072, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "NeoPixel Visualizer initialized on GPIO %d (mode: %d, brightness: %d%%)",
             gpio_num, s_config.mode, s_config.brightness);
    return ESP_OK;
}

void neopixel_set_config(const neopixel_config_t *cfg)
{
    if (!cfg) return;
    s_config.mode = cfg->mode % NEOPIXEL_MODE_COUNT;
    s_config.brightness = cfg->brightness > 100 ? 100 : cfg->brightness;
    save_nvs_config();
}

void neopixel_get_config(neopixel_config_t *out_cfg)
{
    if (out_cfg) {
        *out_cfg = s_config;
    }
}

void neopixel_set_mode(neopixel_mode_t mode)
{
    s_config.mode = mode % NEOPIXEL_MODE_COUNT;
    save_nvs_config();
}

void neopixel_set_brightness(uint8_t brightness)
{
    s_config.brightness = brightness > 100 ? 100 : brightness;
    save_nvs_config();
}
