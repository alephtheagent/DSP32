/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_eq.h"
#include <string.h>

static const float s_default_frequencies[DSP_EQ_NUM_BANDS] = {
    31.0f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

void dsp_eq_init(dsp_eq_t *eq, float sample_rate)
{
    eq->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    eq->config.enabled = true;

    for (size_t i = 0; i < DSP_EQ_NUM_BANDS; i++) {
        dsp_biquad_reset(&eq->filters[i]);
        eq->config.bands[i].freq = s_default_frequencies[i];
        eq->config.bands[i].gain_db = 0.0f;
        eq->config.bands[i].q = 1.414f;
        eq->config.bands[i].enabled = true;

        if (i == 0) {
            eq->config.bands[i].type = DSP_FILTER_LOW_SHELF;
        } else if (i == DSP_EQ_NUM_BANDS - 1) {
            eq->config.bands[i].type = DSP_FILTER_HIGH_SHELF;
        } else {
            eq->config.bands[i].type = DSP_FILTER_PEAKING;
        }

        dsp_biquad_calc(&eq->filters[i],
                        eq->config.bands[i].type,
                        eq->sample_rate,
                        eq->config.bands[i].freq,
                        eq->config.bands[i].gain_db,
                        eq->config.bands[i].q,
                        true);
    }
}

void dsp_eq_set_sample_rate(dsp_eq_t *eq, float sample_rate)
{
    eq->sample_rate = sample_rate;
    for (size_t i = 0; i < DSP_EQ_NUM_BANDS; i++) {
        dsp_biquad_calc(&eq->filters[i],
                        eq->config.bands[i].type,
                        eq->sample_rate,
                        eq->config.bands[i].freq,
                        eq->config.bands[i].gain_db,
                        eq->config.bands[i].q,
                        true);
    }
}

void dsp_eq_set_band(dsp_eq_t *eq, size_t band, float gain_db, float freq, float q, bool immediate)
{
    if (band >= DSP_EQ_NUM_BANDS) return;

    if (freq > 0.0f) eq->config.bands[band].freq = freq;
    if (q > 0.0f) eq->config.bands[band].q = q;
    eq->config.bands[band].gain_db = gain_db;

    dsp_biquad_calc(&eq->filters[band],
                    eq->config.bands[band].type,
                    eq->sample_rate,
                    eq->config.bands[band].freq,
                    eq->config.bands[band].gain_db,
                    eq->config.bands[band].q,
                    immediate);
}

void dsp_eq_update_config(dsp_eq_t *eq, const dsp_eq_config_t *config)
{
    eq->config.enabled = config->enabled;
    for (size_t i = 0; i < DSP_EQ_NUM_BANDS; i++) {
        eq->config.bands[i] = config->bands[i];
        dsp_biquad_calc(&eq->filters[i],
                        eq->config.bands[i].type,
                        eq->sample_rate,
                        eq->config.bands[i].freq,
                        eq->config.bands[i].gain_db,
                        eq->config.bands[i].q,
                        false);
    }
}

void dsp_eq_process(dsp_eq_t *eq, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!eq->config.enabled) return;

    for (size_t i = 0; i < DSP_EQ_NUM_BANDS; i++) {
        if (!eq->config.bands[i].enabled) continue;
        // Optimization: skip biquad math if gain is essentially zero (unless interpolating)
        if (eq->config.bands[i].gain_db == 0.0f && !eq->filters[i].interpolating) {
            continue;
        }
        dsp_biquad_process_stereo(&eq->filters[i], buf_l, buf_r, buf_l, buf_r, num_samples);
    }
}
