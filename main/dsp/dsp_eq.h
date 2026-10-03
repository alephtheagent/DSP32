/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_EQ_H_
#define _DSP_EQ_H_

#include <stddef.h>
#include "dsp_types.h"
#include "dsp_biquad.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_eq_config_t config;
    dsp_biquad_t filters[DSP_EQ_NUM_BANDS];
    float sample_rate;
} dsp_eq_t;

void dsp_eq_init(dsp_eq_t *eq, float sample_rate);
void dsp_eq_set_sample_rate(dsp_eq_t *eq, float sample_rate);
void dsp_eq_set_band(dsp_eq_t *eq, size_t band, float gain_db, float freq, float q, bool immediate);
void dsp_eq_update_config(dsp_eq_t *eq, const dsp_eq_config_t *config);
void dsp_eq_process(dsp_eq_t *eq, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_EQ_H_ */
