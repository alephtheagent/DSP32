/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_deesser.h"
#include <math.h>

void dsp_deesser_init(dsp_deesser_t *deesser, float sample_rate)
{
    deesser->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    deesser->config.enabled = false;
    deesser->config.freq = 6500.0f;
    deesser->config.threshold_db = -24.0f;
    deesser->config.ratio = 4.0f;
    deesser->env_db = -96.0f;

    // Fast attack (1ms), quick release (50ms) for natural sibilance suppression
    deesser->alpha_attack = expf(-1.0f / (0.001f * deesser->sample_rate));
    deesser->alpha_release = expf(-1.0f / (0.050f * deesser->sample_rate));

    dsp_biquad_reset(&deesser->sidechain_bp);
    dsp_biquad_calc(&deesser->sidechain_bp, DSP_FILTER_BAND_PASS, deesser->sample_rate, deesser->config.freq, 0.0f, 2.0f, true);
}

void dsp_deesser_set_sample_rate(dsp_deesser_t *deesser, float sample_rate)
{
    deesser->sample_rate = sample_rate;
    deesser->alpha_attack = expf(-1.0f / (0.001f * deesser->sample_rate));
    deesser->alpha_release = expf(-1.0f / (0.050f * deesser->sample_rate));
    dsp_biquad_calc(&deesser->sidechain_bp, DSP_FILTER_BAND_PASS, deesser->sample_rate, deesser->config.freq, 0.0f, 2.0f, true);
}

void dsp_deesser_update_config(dsp_deesser_t *deesser, const dsp_deesser_config_t *config)
{
    deesser->config = *config;
    dsp_biquad_calc(&deesser->sidechain_bp, DSP_FILTER_BAND_PASS, deesser->sample_rate, deesser->config.freq, 0.0f, 2.0f, false);
}

void dsp_deesser_process(dsp_deesser_t *deesser, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!deesser->config.enabled || num_samples == 0) return;
    if (num_samples > 256) num_samples = 256;

    static float sc_l[256];
    static float sc_r[256];

    // Filter audio through sidechain bandpass to isolate sibilant frequencies
    dsp_biquad_process_stereo(&deesser->sidechain_bp, buf_l, buf_r, sc_l, sc_r, num_samples);

    float T = deesser->config.threshold_db;
    float R = deesser->config.ratio;
    float inv_R = 1.0f / (R > 1.0f ? R : 1.0f);
    float att = deesser->alpha_attack;
    float rel = deesser->alpha_release;
    float env = deesser->env_db;

    for (size_t i = 0; i < num_samples; i++) {
        float peak = fmaxf(fabsf(sc_l[i]), fabsf(sc_r[i]));
        float input_db = (peak > 1e-6f) ? 20.0f * log10f(peak) : -120.0f;

        if (input_db > env) {
            env = att * env + (1.0f - att) * input_db;
        } else {
            env = rel * env + (1.0f - rel) * input_db;
        }

        float gr_db = 0.0f;
        if (env > T) {
            gr_db = (inv_R - 1.0f) * (env - T);
        }

        float gain = powf(10.0f, gr_db / 20.0f);
        buf_l[i] *= gain;
        buf_r[i] *= gain;
    }

    deesser->env_db = env;
}
