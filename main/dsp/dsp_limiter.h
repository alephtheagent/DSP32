/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_LIMITER_H_
#define _DSP_LIMITER_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_LIMITER_LOOKAHEAD_SAMPLES 64

typedef struct {
    dsp_limiter_config_t config;
    float sample_rate;
    float delay_buf_l[DSP_LIMITER_LOOKAHEAD_SAMPLES];
    float delay_buf_r[DSP_LIMITER_LOOKAHEAD_SAMPLES];
    size_t delay_idx;
    float current_gain;
    float alpha_release;
    float ceiling_linear;
} dsp_limiter_t;

void dsp_limiter_init(dsp_limiter_t *lim, float sample_rate);
void dsp_limiter_set_sample_rate(dsp_limiter_t *lim, float sample_rate);
void dsp_limiter_update_config(dsp_limiter_t *lim, const dsp_limiter_config_t *config);
void dsp_limiter_process(dsp_limiter_t *lim, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_LIMITER_H_ */
