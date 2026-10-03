/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_loudness.h"
#include <math.h>

void dsp_loudness_init(dsp_loudness_t *agc, float sample_rate)
{
    agc->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    agc->config.enabled = false;
    agc->config.target_db = -16.0f;
    agc->config.gate_db = -48.0f;
    agc->config.speed = 0.5f;
    agc->current_gain = 1.0f;
    agc->short_term_rms = 0.01f;
}

void dsp_loudness_set_sample_rate(dsp_loudness_t *agc, float sample_rate)
{
    agc->sample_rate = sample_rate;
}

void dsp_loudness_update_config(dsp_loudness_t *agc, const dsp_loudness_config_t *config)
{
    agc->config = *config;
}

void dsp_loudness_process(dsp_loudness_t *agc, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!agc->config.enabled || num_samples == 0) return;

    // Calculate block RMS
    float sum_sq = 0.0f;
    for (size_t i = 0; i < num_samples; i++) {
        sum_sq += buf_l[i] * buf_l[i] + buf_r[i] * buf_r[i];
    }
    float block_rms = sqrtf(sum_sq / (float)(2 * num_samples));

    // Smooth RMS measurement (~300ms window)
    float alpha = expf(-1.0f / (0.3f * (agc->sample_rate / (float)num_samples)));
    agc->short_term_rms = alpha * agc->short_term_rms + (1.0f - alpha) * block_rms;

    float current_db = (agc->short_term_rms > 1e-6f) ? 20.0f * log10f(agc->short_term_rms) : -120.0f;
    float target_gain = agc->current_gain;

    // Only adjust if above noise gate
    if (current_db > agc->config.gate_db) {
        float error_db = agc->config.target_db - current_db;
        // Limit max boost to +12 dB and max cut to -18 dB
        if (error_db > 12.0f) error_db = 12.0f;
        if (error_db < -18.0f) error_db = -18.0f;

        target_gain = powf(10.0f, error_db / 20.0f);
    } else {
        // In silence: smoothly return to unity gain (1.0)
        target_gain = 1.0f;
    }

    // Slew rate control per sample
    float slew_factor = 0.0001f * agc->config.speed;
    float g = agc->current_gain;

    for (size_t i = 0; i < num_samples; i++) {
        g += (target_gain - g) * slew_factor;
        buf_l[i] *= g;
        buf_r[i] *= g;
    }

    agc->current_gain = g;
}
