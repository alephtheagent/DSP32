/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_METER_H_
#define _DSP_METER_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_METER_WAVE_LEN 256

typedef struct {
    float in_peak_l, in_peak_r;
    float in_rms_l, in_rms_r;
    float out_peak_l, out_peak_r;
    float out_rms_l, out_rms_r;
    float sample_rate;
    float decay_factor;
    float wave_buf[DSP_METER_WAVE_LEN];
    uint16_t wave_idx;
} dsp_meter_t;

void dsp_meter_init(dsp_meter_t *meter, float sample_rate);
void dsp_meter_set_sample_rate(dsp_meter_t *meter, float sample_rate);
void dsp_meter_update_input(dsp_meter_t *meter, const float *buf_l, const float *buf_r, size_t num_samples);
void dsp_meter_update_output(dsp_meter_t *meter, const float *buf_l, const float *buf_r, size_t num_samples);
void dsp_meter_update_waveform(dsp_meter_t *meter, const float *buf_l, const float *buf_r, size_t num_samples, float gain);
void dsp_meter_update_pcm(dsp_meter_t *meter, const uint8_t *pcm, size_t bytes, uint8_t bit_depth);
void dsp_meter_get_values(dsp_meter_t *meter, dsp_meter_values_t *out_values);
void dsp_meter_get_waveform(dsp_meter_t *meter, float *out_samples, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_METER_H_ */
