/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_LOUDNESS_H_
#define _DSP_LOUDNESS_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_loudness_config_t config;
    float sample_rate;
    float current_gain;
    float short_term_rms;
} dsp_loudness_t;

void dsp_loudness_init(dsp_loudness_t *agc, float sample_rate);
void dsp_loudness_set_sample_rate(dsp_loudness_t *agc, float sample_rate);
void dsp_loudness_update_config(dsp_loudness_t *agc, const dsp_loudness_config_t *config);
void dsp_loudness_process(dsp_loudness_t *agc, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_LOUDNESS_H_ */
