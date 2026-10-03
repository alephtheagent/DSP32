/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_BIQUAD_H_
#define _DSP_BIQUAD_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    // Direct Form II Transposed state for Left & Right channels
    float s1_l, s2_l;
    float s1_r, s2_r;
    // Target coefficients for smooth parameter interpolation
    float target_b0, target_b1, target_b2;
    float target_a1, target_a2;
    bool interpolating;
} dsp_biquad_t;

void dsp_biquad_reset(dsp_biquad_t *f);
void dsp_biquad_calc(dsp_biquad_t *f, dsp_filter_type_t type, float sample_rate, float freq, float gain_db, float q, bool immediate);
void dsp_biquad_process_stereo(dsp_biquad_t *f, const float *in_l, const float *in_r, float *out_l, float *out_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_BIQUAD_H_ */
