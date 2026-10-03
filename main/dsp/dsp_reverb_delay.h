/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_REVERB_DELAY_H_
#define _DSP_REVERB_DELAY_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FREEVERB_NUM_COMBS    8
#define FREEVERB_NUM_ALLPASS  4

typedef struct {
    float *buffer;
    size_t size;
    size_t idx;
    float filter_store;
} dsp_comb_filter_t;

typedef struct {
    float *buffer;
    size_t size;
    size_t idx;
} dsp_allpass_filter_t;

typedef struct {
    dsp_reverb_delay_config_t config;
    float sample_rate;

    // Delay line buffers allocated in PSRAM
    float *delay_buf_l;
    float *delay_buf_r;
    size_t delay_max_samples;
    size_t delay_idx_l;
    size_t delay_idx_r;
    float delay_damp_l;
    float delay_damp_r;

    // Freeverb structures
    dsp_comb_filter_t    comb_l[FREEVERB_NUM_COMBS];
    dsp_comb_filter_t    comb_r[FREEVERB_NUM_COMBS];
    dsp_allpass_filter_t allpass_l[FREEVERB_NUM_ALLPASS];
    dsp_allpass_filter_t allpass_r[FREEVERB_NUM_ALLPASS];
    bool initialized;
} dsp_reverb_delay_t;

void dsp_reverb_delay_init(dsp_reverb_delay_t *rd, float sample_rate);
void dsp_reverb_delay_set_sample_rate(dsp_reverb_delay_t *rd, float sample_rate);
void dsp_reverb_delay_update_config(dsp_reverb_delay_t *rd, const dsp_reverb_delay_config_t *config);
void dsp_reverb_delay_process(dsp_reverb_delay_t *rd, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_REVERB_DELAY_H_ */
