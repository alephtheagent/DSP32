/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_tube_warmth.h"
#include <math.h>

void dsp_tube_warmth_init(dsp_tube_warmth_t *tube)
{
    tube->config.enabled = false;
    tube->config.drive = 0.35f;
    tube->config.warmth = 0.5f;
    tube->dc_x_l = tube->dc_y_l = 0.0f;
    tube->dc_x_r = tube->dc_y_r = 0.0f;
}

void dsp_tube_warmth_update_config(dsp_tube_warmth_t *tube, const dsp_tube_config_t *config)
{
    tube->config = *config;
}

static inline float saturate_sample(float in, float drive, float warmth)
{
    // Pre-gain boost
    float x = in * (1.0f + drive * 2.5f);

    // Asymmetric quadratic component (even harmonic - warm tube)
    float even = 0.25f * warmth * (x * fabsf(x));
    float x_biased = x + even;

    // Soft saturation curve: tanh-like rational function
    float y = x_biased / (1.0f + fabsf(x_biased));

    // Post-gain normalization
    float norm = 1.0f / (1.0f + drive * 1.5f);
    return y * (1.0f + (1.0f - norm) * 0.5f);
}

void dsp_tube_warmth_process(dsp_tube_warmth_t *tube, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!tube->config.enabled) return;

    float drive = tube->config.drive;
    float warmth = tube->config.warmth;
    float dc_xl = tube->dc_x_l, dc_yl = tube->dc_y_l;
    float dc_xr = tube->dc_x_r, dc_yr = tube->dc_y_r;
    const float R = 0.995f; // DC-blocker pole (~15Hz at 48kHz)

    for (size_t i = 0; i < num_samples; i++) {
        float sl = saturate_sample(buf_l[i], drive, warmth);
        float sr = saturate_sample(buf_r[i], drive, warmth);

        // DC-blocking filter: y[n] = x[n] - x[n-1] + R * y[n-1]
        float yl = sl - dc_xl + R * dc_yl;
        dc_xl = sl;
        dc_yl = yl;

        float yr = sr - dc_xr + R * dc_yr;
        dc_xr = sr;
        dc_yr = yr;

        buf_l[i] = yl;
        buf_r[i] = yr;
    }

    // Flush denormals
    if (fabsf(dc_yl) < 1e-15f) dc_yl = 0.0f;
    if (fabsf(dc_yr) < 1e-15f) dc_yr = 0.0f;

    tube->dc_x_l = dc_xl; tube->dc_y_l = dc_yl;
    tube->dc_x_r = dc_xr; tube->dc_y_r = dc_yr;
}
