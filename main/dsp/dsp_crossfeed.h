/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_CROSSFEED_H_
#define _DSP_CROSSFEED_H_

#include <stddef.h>
#include "dsp_types.h"
#include "dsp_biquad.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_CROSSFEED_MAX_DELAY 64

typedef struct {
    dsp_crossfeed_config_t config;
    dsp_biquad_t lpf;
    float delay_l[DSP_CROSSFEED_MAX_DELAY];
    float delay_r[DSP_CROSSFEED_MAX_DELAY];
    size_t delay_idx;
    size_t delay_samples;
    float sample_rate;
} dsp_crossfeed_t;

void dsp_crossfeed_init(dsp_crossfeed_t *cf, float sample_rate);
void dsp_crossfeed_set_sample_rate(dsp_crossfeed_t *cf, float sample_rate);
void dsp_crossfeed_update_config(dsp_crossfeed_t *cf, const dsp_crossfeed_config_t *config);
void dsp_crossfeed_process(dsp_crossfeed_t *cf, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_CROSSFEED_H_ */
