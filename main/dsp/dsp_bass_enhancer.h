/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_BASS_ENHANCER_H_
#define _DSP_BASS_ENHANCER_H_

#include <stddef.h>
#include "dsp_types.h"
#include "dsp_biquad.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_bass_config_t config;
    dsp_biquad_t sub_lpf;
    dsp_biquad_t harm_bpf;
    float sample_rate;
} dsp_bass_enhancer_t;

void dsp_bass_enhancer_init(dsp_bass_enhancer_t *bass, float sample_rate);
void dsp_bass_enhancer_set_sample_rate(dsp_bass_enhancer_t *bass, float sample_rate);
void dsp_bass_enhancer_update_config(dsp_bass_enhancer_t *bass, const dsp_bass_config_t *config);
void dsp_bass_enhancer_process(dsp_bass_enhancer_t *bass, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_BASS_ENHANCER_H_ */
