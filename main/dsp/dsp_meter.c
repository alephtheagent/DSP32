/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_meter.h"
#include <math.h>
#include "storage/hw_config.h"

void dsp_meter_init(dsp_meter_t *meter, float sample_rate)
{
    meter->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    meter->in_peak_l = meter->in_peak_r = 0.0f;
    meter->in_rms_l = meter->in_rms_r = 0.0f;
    meter->out_peak_l = meter->out_peak_r = 0.0f;
    meter->out_rms_l = meter->out_rms_r = 0.0f;
    meter->wave_idx = 0;
    for (int i = 0; i < DSP_METER_WAVE_LEN; i++) {
        meter->wave_buf[i] = 0.0f;
    }
    // ~300ms release ballistics
    meter->decay_factor = expf(-1.0f / (0.300f * (meter->sample_rate / 64.0f)));
}

void dsp_meter_set_sample_rate(dsp_meter_t *meter, float sample_rate)
{
    meter->sample_rate = sample_rate;
    meter->decay_factor = expf(-1.0f / (0.300f * (meter->sample_rate / 64.0f)));
}

static inline float lin_to_db(float lin)
{
    if (lin < 1e-5f) return -100.0f;
    float db = 20.0f * log10f(lin);
    if (db > 0.0f) db = 0.0f;
    if (db < -100.0f) db = -100.0f;
    return db;
}

void dsp_meter_update_input(dsp_meter_t *meter, const float *buf_l, const float *buf_r, size_t num_samples)
{
    if (num_samples == 0) return;

    float max_l = 0.0f, max_r = 0.0f;
    float sum_l = 0.0f, sum_r = 0.0f;

    for (size_t i = 0; i < num_samples; i++) {
        float al = fabsf(buf_l[i]);
        float ar = fabsf(buf_r[i]);
        if (al > max_l) max_l = al;
        if (ar > max_r) max_r = ar;
        sum_l += al * al;
        sum_r += ar * ar;
    }

    float rms_l = sqrtf(sum_l / (float)num_samples);
    float rms_r = sqrtf(sum_r / (float)num_samples);

    float decay = meter->decay_factor;
    meter->in_peak_l = (max_l > meter->in_peak_l) ? max_l : (meter->in_peak_l * decay);
    meter->in_peak_r = (max_r > meter->in_peak_r) ? max_r : (meter->in_peak_r * decay);
    meter->in_rms_l = (rms_l > meter->in_rms_l) ? rms_l : (meter->in_rms_l * decay);
    meter->in_rms_r = (rms_r > meter->in_rms_r) ? rms_r : (meter->in_rms_r * decay);
}

void dsp_meter_update_output(dsp_meter_t *meter, const float *buf_l, const float *buf_r, size_t num_samples)
{
    if (num_samples == 0) return;

    float max_l = 0.0f, max_r = 0.0f;
    float sum_l = 0.0f, sum_r = 0.0f;

    for (size_t i = 0; i < num_samples; i++) {
        float al = fabsf(buf_l[i]);
        float ar = fabsf(buf_r[i]);
        if (al > max_l) max_l = al;
        if (ar > max_r) max_r = ar;
        sum_l += al * al;
        sum_r += ar * ar;
    }

    float rms_l = sqrtf(sum_l / (float)num_samples);
    float rms_r = sqrtf(sum_r / (float)num_samples);

    float decay = meter->decay_factor;
    meter->out_peak_l = (max_l > meter->out_peak_l) ? max_l : (meter->out_peak_l * decay);
    meter->out_peak_r = (max_r > meter->out_peak_r) ? max_r : (meter->out_peak_r * decay);
    meter->out_rms_l = (rms_l > meter->out_rms_l) ? rms_l : (meter->out_rms_l * decay);
    meter->out_rms_r = (rms_r > meter->out_rms_r) ? rms_r : (meter->out_rms_r * decay);
}

void dsp_meter_update_waveform(dsp_meter_t *meter, const float *buf_l, const float *buf_r, size_t num_samples, float gain)
{
    if (!meter || num_samples == 0) return;
    if (gain <= 0.0f) gain = 1.0f;

    for (size_t i = 0; i < num_samples; i++) {
        float mono = (buf_l[i] + buf_r[i]) * 0.5f * gain;
        if (mono > 1.0f) mono = 1.0f;
        else if (mono < -1.0f) mono = -1.0f;

        meter->wave_buf[meter->wave_idx++] = mono;
        if (meter->wave_idx >= DSP_METER_WAVE_LEN) {
            meter->wave_idx = 0;
        }
    }
}

void dsp_meter_update_pcm(dsp_meter_t *meter, const uint8_t *pcm, size_t bytes, uint8_t bit_depth)
{
    if (bytes == 0 || !pcm) return;
    size_t bytes_per_sample = (bit_depth == 24) ? 4 : 2;
    size_t num_samples = bytes / (2 * bytes_per_sample);
    if (num_samples == 0) return;

    float max_l = 0.0f, max_r = 0.0f;
    float sum_l = 0.0f, sum_r = 0.0f;
    const hw_config_t *hw = hw_config_get();
    float scope_gain = hw ? hw->scope_gain : 2.0f;

    if (bit_depth == 16) {
        const int16_t *src = (const int16_t *)pcm;
        const float norm16 = 1.0f / 32768.0f;
        for (size_t i = 0; i < num_samples; i++) {
            float l = (float)src[2 * i] * norm16;
            float r = (float)src[2 * i + 1] * norm16;
            float mono = (l + r) * 0.5f * scope_gain;
            if (mono > 1.0f) mono = 1.0f;
            else if (mono < -1.0f) mono = -1.0f;
            meter->wave_buf[meter->wave_idx++] = mono;
            if (meter->wave_idx >= DSP_METER_WAVE_LEN) {
                meter->wave_idx = 0;
            }
        }
        for (size_t i = 0; i < num_samples; i++) {
            float al = fabsf((float)src[2 * i] * norm16);
            float ar = fabsf((float)src[2 * i + 1] * norm16);
            if (al > max_l) max_l = al;
            if (ar > max_r) max_r = ar;
            sum_l += al * al;
            sum_r += ar * ar;
        }
    } else {
        const int32_t *src = (const int32_t *)pcm;
        const float norm24 = 1.0f / 8388608.0f;
        for (size_t i = 0; i < num_samples; i++) {
            float l = (float)(src[2 * i] >> 8) * norm24;
            float r = (float)(src[2 * i + 1] >> 8) * norm24;
            float mono = (l + r) * 0.5f * scope_gain;
            if (mono > 1.0f) mono = 1.0f;
            else if (mono < -1.0f) mono = -1.0f;
            meter->wave_buf[meter->wave_idx++] = mono;
            if (meter->wave_idx >= DSP_METER_WAVE_LEN) {
                meter->wave_idx = 0;
            }
        }
        for (size_t i = 0; i < num_samples; i++) {
            float al = fabsf((float)(src[2 * i] >> 8) * norm24);
            float ar = fabsf((float)(src[2 * i + 1] >> 8) * norm24);
            if (al > max_l) max_l = al;
            if (ar > max_r) max_r = ar;
            sum_l += al * al;
            sum_r += ar * ar;
        }
    }

    float rms_l = sqrtf(sum_l / (float)num_samples);
    float rms_r = sqrtf(sum_r / (float)num_samples);
    float decay = meter->decay_factor;

    meter->in_peak_l = (max_l > meter->in_peak_l) ? max_l : (meter->in_peak_l * decay);
    meter->in_peak_r = (max_r > meter->in_peak_r) ? max_r : (meter->in_peak_r * decay);
    meter->in_rms_l = (rms_l > meter->in_rms_l) ? rms_l : (meter->in_rms_l * decay);
    meter->in_rms_r = (rms_r > meter->in_rms_r) ? rms_r : (meter->in_rms_r * decay);

    // In bit-perfect mode, output is identical to input
    meter->out_peak_l = meter->in_peak_l;
    meter->out_peak_r = meter->in_peak_r;
    meter->out_rms_l = meter->in_rms_l;
    meter->out_rms_r = meter->in_rms_r;
}

void dsp_meter_get_values(dsp_meter_t *meter, dsp_meter_values_t *out_values)
{
    if (!out_values) return;
    out_values->in_peak_l = lin_to_db(meter->in_peak_l);
    out_values->in_peak_r = lin_to_db(meter->in_peak_r);
    out_values->in_rms_l = lin_to_db(meter->in_rms_l);
    out_values->in_rms_r = lin_to_db(meter->in_rms_r);
    out_values->out_peak_l = lin_to_db(meter->out_peak_l);
    out_values->out_peak_r = lin_to_db(meter->out_peak_r);
    out_values->out_rms_l = lin_to_db(meter->out_rms_l);
    out_values->out_rms_r = lin_to_db(meter->out_rms_r);
}

void dsp_meter_get_waveform(dsp_meter_t *meter, float *out_samples, size_t count)
{
    if (!meter || !out_samples || count == 0) return;
    if (count > DSP_METER_WAVE_LEN) count = DSP_METER_WAVE_LEN;

    // Decay towards silence if idle/silent across both input and output (-80 dBFS)
    if (meter->in_peak_l < 0.0001f && meter->in_peak_r < 0.0001f &&
        meter->out_peak_l < 0.0001f && meter->out_peak_r < 0.0001f) {
        for (size_t i = 0; i < DSP_METER_WAVE_LEN; i++) {
            meter->wave_buf[i] *= 0.85f;
        }
        for (size_t i = 0; i < count; i++) {
            out_samples[i] = 0.0f;
        }
        return;
    }

    // Trigger Search: look for rising zero-crossing in oldest portion
    uint16_t start = meter->wave_idx; // Oldest sample in circular buffer
    uint16_t trigger = start;
    bool found_trigger = false;

    size_t search_limit = (DSP_METER_WAVE_LEN > count) ? (DSP_METER_WAVE_LEN - count) : 0;
    for (size_t i = 0; i < search_limit; i++) {
        uint16_t idx1 = (start + i) % DSP_METER_WAVE_LEN;
        uint16_t idx2 = (start + i + 1) % DSP_METER_WAVE_LEN;
        if (meter->wave_buf[idx1] <= 0.0f && meter->wave_buf[idx2] > 0.0f) {
            trigger = idx2;
            found_trigger = true;
            break;
        }
    }

    if (!found_trigger) {
        trigger = start;
    }

    for (size_t i = 0; i < count; i++) {
        uint16_t idx = (trigger + i) % DSP_METER_WAVE_LEN;
        out_samples[i] = meter->wave_buf[idx];
    }
}
