/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_PITCH_H_
#define _DSP_PITCH_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_PITCH_BUFFER_SIZE 2048

typedef struct {
    dsp_pitch_config_t config;
    float sample_rate;
    float buf_l[DSP_PITCH_BUFFER_SIZE];
    float buf_r[DSP_PITCH_BUFFER_SIZE];
    size_t write_idx;
    float phase;
    float pitch_factor;
} dsp_pitch_t;

void dsp_pitch_init(dsp_pitch_t *pitch, float sample_rate);
void dsp_pitch_set_sample_rate(dsp_pitch_t *pitch, float sample_rate);
void dsp_pitch_update_config(dsp_pitch_t *pitch, const dsp_pitch_config_t *config);
void dsp_pitch_process(dsp_pitch_t *pitch, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_PITCH_H_ */
