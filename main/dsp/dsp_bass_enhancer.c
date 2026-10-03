/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_bass_enhancer.h"
#include <math.h>

void dsp_bass_enhancer_init(dsp_bass_enhancer_t *bass, float sample_rate)
{
    bass->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    bass->config.enabled = false;
    bass->config.cutoff_hz = 90.0f;
    bass->config.drive = 0.5f;
    bass->config.blend = 0.4f;

    dsp_biquad_reset(&bass->sub_lpf);
    dsp_biquad_reset(&bass->harm_bpf);

    dsp_biquad_calc(&bass->sub_lpf, DSP_FILTER_LOW_PASS, bass->sample_rate, bass->config.cutoff_hz, 0.0f, 0.707f, true);
    // Bandpass for harmonics centered around 2x cutoff
    dsp_biquad_calc(&bass->harm_bpf, DSP_FILTER_BAND_PASS, bass->sample_rate, bass->config.cutoff_hz * 2.0f, 0.0f, 1.2f, true);
}

void dsp_bass_enhancer_set_sample_rate(dsp_bass_enhancer_t *bass, float sample_rate)
{
    bass->sample_rate = sample_rate;
    dsp_biquad_calc(&bass->sub_lpf, DSP_FILTER_LOW_PASS, bass->sample_rate, bass->config.cutoff_hz, 0.0f, 0.707f, true);
    dsp_biquad_calc(&bass->harm_bpf, DSP_FILTER_BAND_PASS, bass->sample_rate, bass->config.cutoff_hz * 2.0f, 0.0f, 1.2f, true);
}

void dsp_bass_enhancer_update_config(dsp_bass_enhancer_t *bass, const dsp_bass_config_t *config)
{
    bass->config = *config;
    dsp_biquad_calc(&bass->sub_lpf, DSP_FILTER_LOW_PASS, bass->sample_rate, bass->config.cutoff_hz, 0.0f, 0.707f, false);
    dsp_biquad_calc(&bass->harm_bpf, DSP_FILTER_BAND_PASS, bass->sample_rate, bass->config.cutoff_hz * 2.0f, 0.0f, 1.2f, false);
}

void dsp_bass_enhancer_process(dsp_bass_enhancer_t *bass, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!bass->config.enabled || num_samples == 0) return;
    if (num_samples > 256) num_samples = 256;

    static float sub_l[256];
    static float sub_r[256];
    static float harm_l[256];
    static float harm_r[256];

    // Isolate sub-bass (< 90Hz)
    dsp_biquad_process_stereo(&bass->sub_lpf, buf_l, buf_r, sub_l, sub_r, num_samples);

    float drive = 1.0f + bass->config.drive * 4.0f;

    // Non-linear harmonic generation: synthesizes 2nd and 3rd harmonics
    for (size_t i = 0; i < num_samples; i++) {
        float xl = sub_l[i] * drive;
        float xr = sub_r[i] * drive;

        // Even harmonic: x * |x|, Odd harmonic: x^3 soft clipping
        harm_l[i] = (xl * fabsf(xl) * 0.6f) + (xl - (xl * xl * xl * 0.15f)) * 0.4f;
        harm_r[i] = (xr * fabsf(xr) * 0.6f) + (xr - (xr * xr * xr * 0.15f)) * 0.4f;
    }

    // Filter generated harmonics to remove unwanted rumble and harshness
    dsp_biquad_process_stereo(&bass->harm_bpf, harm_l, harm_r, harm_l, harm_r, num_samples);

    float blend = bass->config.blend;
    for (size_t i = 0; i < num_samples; i++) {
        buf_l[i] += harm_l[i] * blend;
        buf_r[i] += harm_r[i] * blend;
    }
}
