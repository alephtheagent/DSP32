/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_compressor.h"
#include <math.h>

static void calc_ballistics(dsp_compressor_t *comp)
{
    float att_sec = comp->config.attack_ms * 0.001f;
    float rel_sec = comp->config.release_ms * 0.001f;
    if (att_sec < 0.0001f) att_sec = 0.0001f;
    if (rel_sec < 0.001f) rel_sec = 0.001f;

    comp->alpha_attack = expf(-1.0f / (att_sec * comp->sample_rate));
    comp->alpha_release = expf(-1.0f / (rel_sec * comp->sample_rate));
    comp->makeup_linear = powf(10.0f, comp->config.makeup_db / 20.0f);
}

void dsp_compressor_init(dsp_compressor_t *comp, float sample_rate)
{
    comp->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    comp->config.enabled = false;
    comp->config.threshold_db = -20.0f;
    comp->config.ratio = 4.0f;
    comp->config.attack_ms = 15.0f;
    comp->config.release_ms = 100.0f;
    comp->config.knee_db = 6.0f;
    comp->config.makeup_db = 0.0f;
    comp->env_db = -96.0f;

    calc_ballistics(comp);
}

void dsp_compressor_set_sample_rate(dsp_compressor_t *comp, float sample_rate)
{
    comp->sample_rate = sample_rate;
    calc_ballistics(comp);
}

void dsp_compressor_update_config(dsp_compressor_t *comp, const dsp_compressor_config_t *config)
{
    comp->config = *config;
    calc_ballistics(comp);
}

void dsp_compressor_process(dsp_compressor_t *comp, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!comp->config.enabled) return;

    float T = comp->config.threshold_db;
    float R = comp->config.ratio;
    float W = comp->config.knee_db;
    float inv_R = 1.0f / (R > 1.0f ? R : 1.0f);
    float att = comp->alpha_attack;
    float rel = comp->alpha_release;
    float makeup = comp->makeup_linear;
    float env_db = comp->env_db;

    for (size_t i = 0; i < num_samples; i++) {
        float peak = fmaxf(fabsf(buf_l[i]), fabsf(buf_r[i]));
        float input_db = (peak > 1e-6f) ? 20.0f * log10f(peak) : -120.0f;

        // Ballistics smoothing
        if (input_db > env_db) {
            env_db = att * env_db + (1.0f - att) * input_db;
        } else {
            env_db = rel * env_db + (1.0f - rel) * input_db;
        }

        // Static compression curve with soft knee
        float gr_db = 0.0f;
        float diff = env_db - T;

        if (W > 0.0f && fabsf(diff) <= (W * 0.5f)) {
            // In the soft knee region
            float knee_val = diff + W * 0.5f;
            gr_db = (inv_R - 1.0f) * (knee_val * knee_val) / (2.0f * W);
        } else if (diff > (W * 0.5f)) {
            // Above knee: full ratio
            gr_db = (inv_R - 1.0f) * diff;
        } else {
            // Below knee: no gain reduction
            gr_db = 0.0f;
        }

        // Convert gain reduction from dB to linear multiplier
        float gain = powf(10.0f, gr_db / 20.0f) * makeup;

        buf_l[i] *= gain;
        buf_r[i] *= gain;
    }

    comp->env_db = env_db;
}
