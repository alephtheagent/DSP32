/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_limiter.h"
#include <math.h>
#include <string.h>

static void calc_limiter_params(dsp_limiter_t *lim)
{
    float rel_sec = lim->config.release_ms * 0.001f;
    if (rel_sec < 0.001f) rel_sec = 0.001f;
    lim->alpha_release = expf(-1.0f / (rel_sec * lim->sample_rate));
    lim->ceiling_linear = powf(10.0f, lim->config.ceiling_db / 20.0f);
}

void dsp_limiter_init(dsp_limiter_t *lim, float sample_rate)
{
    lim->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    lim->config.enabled = true;
    lim->config.ceiling_db = -0.2f;
    lim->config.release_ms = 50.0f;
    lim->delay_idx = 0;
    lim->current_gain = 1.0f;
    memset(lim->delay_buf_l, 0, sizeof(lim->delay_buf_l));
    memset(lim->delay_buf_r, 0, sizeof(lim->delay_buf_r));

    calc_limiter_params(lim);
}

void dsp_limiter_set_sample_rate(dsp_limiter_t *lim, float sample_rate)
{
    lim->sample_rate = sample_rate;
    calc_limiter_params(lim);
}

void dsp_limiter_update_config(dsp_limiter_t *lim, const dsp_limiter_config_t *config)
{
    lim->config = *config;
    calc_limiter_params(lim);
}

void dsp_limiter_process(dsp_limiter_t *lim, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!lim->config.enabled) return;

    float ceiling = lim->ceiling_linear;
    float rel = lim->alpha_release;
    float gain = lim->current_gain;
    size_t idx = lim->delay_idx;

    for (size_t i = 0; i < num_samples; i++) {
        float in_l = buf_l[i];
        float in_r = buf_r[i];

        // Store into lookahead circular buffer
        float delayed_l = lim->delay_buf_l[idx];
        float delayed_r = lim->delay_buf_r[idx];
        lim->delay_buf_l[idx] = in_l;
        lim->delay_buf_r[idx] = in_r;
        idx = (idx + 1) % DSP_LIMITER_LOOKAHEAD_SAMPLES;

        // Check the peak of the incoming sample (ahead in time)
        float peak = fmaxf(fabsf(in_l), fabsf(in_r));
        float target_gain = 1.0f;
        if (peak > ceiling && peak > 1e-6f) {
            target_gain = ceiling / peak;
        }

        // Fast attack / smooth release envelope
        if (target_gain < gain) {
            // Immediate lookahead attack reduction
            gain = target_gain;
        } else {
            // Exponential release recovery
            gain = rel * gain + (1.0f - rel) * 1.0f;
        }

        // Apply gain to the delayed audio sample
        float out_l = delayed_l * gain;
        float out_r = delayed_r * gain;

        // Hard safety clamp at ceiling
        if (out_l > ceiling) out_l = ceiling;
        else if (out_l < -ceiling) out_l = -ceiling;

        if (out_r > ceiling) out_r = ceiling;
        else if (out_r < -ceiling) out_r = -ceiling;

        buf_l[i] = out_l;
        buf_r[i] = out_r;
    }

    lim->current_gain = gain;
    lim->delay_idx = idx;
}
