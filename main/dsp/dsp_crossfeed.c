/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_crossfeed.h"
#include <string.h>

void dsp_crossfeed_init(dsp_crossfeed_t *cf, float sample_rate)
{
    cf->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    cf->config.enabled = false;
    cf->config.amount = 0.6f;
    cf->config.cutoff_hz = 700.0f;
    cf->delay_idx = 0;
    // ~280 microseconds interaural time difference (ITD)
    cf->delay_samples = (size_t)(cf->sample_rate * 0.00028f);
    if (cf->delay_samples >= DSP_CROSSFEED_MAX_DELAY) cf->delay_samples = DSP_CROSSFEED_MAX_DELAY - 1;
    if (cf->delay_samples < 1) cf->delay_samples = 1;

    memset(cf->delay_l, 0, sizeof(cf->delay_l));
    memset(cf->delay_r, 0, sizeof(cf->delay_r));

    dsp_biquad_reset(&cf->lpf);
    dsp_biquad_calc(&cf->lpf, DSP_FILTER_LOW_PASS, cf->sample_rate, cf->config.cutoff_hz, 0.0f, 0.707f, true);
}

void dsp_crossfeed_set_sample_rate(dsp_crossfeed_t *cf, float sample_rate)
{
    cf->sample_rate = sample_rate;
    cf->delay_samples = (size_t)(cf->sample_rate * 0.00028f);
    if (cf->delay_samples >= DSP_CROSSFEED_MAX_DELAY) cf->delay_samples = DSP_CROSSFEED_MAX_DELAY - 1;
    if (cf->delay_samples < 1) cf->delay_samples = 1;

    dsp_biquad_calc(&cf->lpf, DSP_FILTER_LOW_PASS, cf->sample_rate, cf->config.cutoff_hz, 0.0f, 0.707f, true);
}

void dsp_crossfeed_update_config(dsp_crossfeed_t *cf, const dsp_crossfeed_config_t *config)
{
    cf->config = *config;
    dsp_biquad_calc(&cf->lpf, DSP_FILTER_LOW_PASS, cf->sample_rate, cf->config.cutoff_hz, 0.0f, 0.707f, false);
}

void dsp_crossfeed_process(dsp_crossfeed_t *cf, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!cf->config.enabled || num_samples == 0) return;
    if (num_samples > 256) num_samples = 256;

    static float filtered_l[256];
    static float filtered_r[256];

    // Filter channels with 700Hz LPF
    dsp_biquad_process_stereo(&cf->lpf, buf_l, buf_r, filtered_l, filtered_r, num_samples);

    float amount = cf->config.amount * 0.35f; // Scale crossfeed gain naturally (-9dB to -6dB range)
    size_t idx = cf->delay_idx;
    size_t delay = cf->delay_samples;

    for (size_t i = 0; i < num_samples; i++) {
        // Read delayed opposite channels
        size_t read_idx = (idx + DSP_CROSSFEED_MAX_DELAY - delay) % DSP_CROSSFEED_MAX_DELAY;
        float delayed_r_to_l = cf->delay_r[read_idx];
        float delayed_l_to_r = cf->delay_l[read_idx];

        // Store current filtered sample into delay lines
        cf->delay_l[idx] = filtered_l[i];
        cf->delay_r[idx] = filtered_r[i];
        idx = (idx + 1) % DSP_CROSSFEED_MAX_DELAY;

        // Blend crossfeed
        buf_l[i] = buf_l[i] * (1.0f - amount * 0.3f) + delayed_r_to_l * amount;
        buf_r[i] = buf_r[i] * (1.0f - amount * 0.3f) + delayed_l_to_r * amount;
    }

    cf->delay_idx = idx;
}
