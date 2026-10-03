/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_reverb_delay.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "DSP_REVERB";

// Classic Freeverb tuning at 44.1 kHz
static const size_t s_comb_tuning_l[FREEVERB_NUM_COMBS] = {
    1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617
};
static const size_t s_allpass_tuning_l[FREEVERB_NUM_ALLPASS] = {
    556, 441, 341, 225
};
#define STEREO_SPREAD 23

static void *alloc_psram_or_sram(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        ptr = malloc(size);
    }
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void dsp_reverb_delay_init(dsp_reverb_delay_t *rd, float sample_rate)
{
    rd->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    rd->config.delay_enabled = false;
    rd->config.delay_time_ms = 350.0f;
    rd->config.delay_feedback = 0.45f;
    rd->config.delay_mix = 0.35f;

    rd->config.reverb_enabled = false;
    rd->config.reverb_room_size = 0.75f;
    rd->config.reverb_damping = 0.3f;
    rd->config.reverb_mix = 0.25f;

    rd->delay_idx_l = 0;
    rd->delay_idx_r = 0;
    rd->delay_damp_l = 0.0f;
    rd->delay_damp_r = 0.0f;

    // 1.0 second max delay at 96kHz = 96000 samples per channel
    rd->delay_max_samples = 96000;
    rd->delay_buf_l = (float *)alloc_psram_or_sram(rd->delay_max_samples * sizeof(float));
    rd->delay_buf_r = (float *)alloc_psram_or_sram(rd->delay_max_samples * sizeof(float));

    float scale = rd->sample_rate / 44100.0f;

    for (size_t i = 0; i < FREEVERB_NUM_COMBS; i++) {
        size_t sz_l = (size_t)((float)s_comb_tuning_l[i] * scale);
        size_t sz_r = (size_t)((float)(s_comb_tuning_l[i] + STEREO_SPREAD) * scale);

        rd->comb_l[i].buffer = (float *)alloc_psram_or_sram(sz_l * sizeof(float));
        rd->comb_l[i].size = sz_l;
        rd->comb_l[i].idx = 0;
        rd->comb_l[i].filter_store = 0.0f;

        rd->comb_r[i].buffer = (float *)alloc_psram_or_sram(sz_r * sizeof(float));
        rd->comb_r[i].size = sz_r;
        rd->comb_r[i].idx = 0;
        rd->comb_r[i].filter_store = 0.0f;
    }

    for (size_t i = 0; i < FREEVERB_NUM_ALLPASS; i++) {
        size_t sz_l = (size_t)((float)s_allpass_tuning_l[i] * scale);
        size_t sz_r = (size_t)((float)(s_allpass_tuning_l[i] + STEREO_SPREAD) * scale);

        rd->allpass_l[i].buffer = (float *)alloc_psram_or_sram(sz_l * sizeof(float));
        rd->allpass_l[i].size = sz_l;
        rd->allpass_l[i].idx = 0;

        rd->allpass_r[i].buffer = (float *)alloc_psram_or_sram(sz_r * sizeof(float));
        rd->allpass_r[i].size = sz_r;
        rd->allpass_r[i].idx = 0;
    }

    rd->initialized = true;
    ESP_LOGI(TAG, "Reverb & Delay initialized with PSRAM backing buffers");
}

void dsp_reverb_delay_set_sample_rate(dsp_reverb_delay_t *rd, float sample_rate)
{
    rd->sample_rate = sample_rate;
}

void dsp_reverb_delay_update_config(dsp_reverb_delay_t *rd, const dsp_reverb_delay_config_t *config)
{
    rd->config = *config;
}

static inline float process_comb(dsp_comb_filter_t *c, float input, float feedback, float damp)
{
    float output = c->buffer[c->idx];
    c->filter_store = (output * (1.0f - damp)) + (c->filter_store * damp);
    c->buffer[c->idx] = input + (c->filter_store * feedback);
    if (++c->idx >= c->size) c->idx = 0;
    return output;
}

static inline float process_allpass(dsp_allpass_filter_t *a, float input)
{
    float buf_out = a->buffer[a->idx];
    float output = -input + buf_out;
    a->buffer[a->idx] = input + (buf_out * 0.5f);
    if (++a->idx >= a->size) a->idx = 0;
    return output;
}

void dsp_reverb_delay_process(dsp_reverb_delay_t *rd, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!rd->initialized) return;

    // 1. Ping-Pong Delay
    if (rd->config.delay_enabled && rd->delay_buf_l && rd->delay_buf_r) {
        size_t delay_len = (size_t)(rd->config.delay_time_ms * 0.001f * rd->sample_rate);
        if (delay_len >= rd->delay_max_samples) delay_len = rd->delay_max_samples - 1;
        if (delay_len < 1) delay_len = 1;

        float fb = rd->config.delay_feedback;
        float mix = rd->config.delay_mix;
        size_t max_s = rd->delay_max_samples;
        size_t idx_l = rd->delay_idx_l;
        size_t idx_r = rd->delay_idx_r;

        for (size_t i = 0; i < num_samples; i++) {
            size_t read_l = (idx_l + max_s - delay_len) % max_s;
            size_t read_r = (idx_r + max_s - delay_len) % max_s;

            float delayed_l = rd->delay_buf_l[read_l];
            float delayed_r = rd->delay_buf_r[read_r];

            // Ping-Pong cross: L writes to R delay with damp, R writes to L delay
            rd->delay_damp_l = 0.7f * rd->delay_damp_l + 0.3f * (buf_l[i] + delayed_r * fb);
            rd->delay_damp_r = 0.7f * rd->delay_damp_r + 0.3f * (buf_r[i] + delayed_l * fb);

            rd->delay_buf_l[idx_l] = rd->delay_damp_l;
            rd->delay_buf_r[idx_r] = rd->delay_damp_r;

            idx_l = (idx_l + 1) % max_s;
            idx_r = (idx_r + 1) % max_s;

            buf_l[i] = buf_l[i] * (1.0f - mix * 0.5f) + delayed_l * mix;
            buf_r[i] = buf_r[i] * (1.0f - mix * 0.5f) + delayed_r * mix;
        }

        rd->delay_idx_l = idx_l;
        rd->delay_idx_r = idx_r;
    }

    // 2. Freeverb Algorithmic Reverb
    if (rd->config.reverb_enabled) {
        float room = rd->config.reverb_room_size * 0.28f + 0.7f;
        float damp = rd->config.reverb_damping * 0.4f;
        float wet = rd->config.reverb_mix * 0.5f;
        float dry = 1.0f - wet * 0.5f;

        for (size_t i = 0; i < num_samples; i++) {
            float in = (buf_l[i] + buf_r[i]) * 0.015f; // Input attenuation for headroom

            float out_l = 0.0f;
            float out_r = 0.0f;

            // 8 parallel combs
            for (size_t c = 0; c < FREEVERB_NUM_COMBS; c++) {
                out_l += process_comb(&rd->comb_l[c], in, room, damp);
                out_r += process_comb(&rd->comb_r[c], in, room, damp);
            }

            // 4 series allpasses
            for (size_t a = 0; a < FREEVERB_NUM_ALLPASS; a++) {
                out_l = process_allpass(&rd->allpass_l[a], out_l);
                out_r = process_allpass(&rd->allpass_r[a], out_r);
            }

            buf_l[i] = buf_l[i] * dry + out_l * wet;
            buf_r[i] = buf_r[i] * dry + out_r * wet;
        }
    }
}
