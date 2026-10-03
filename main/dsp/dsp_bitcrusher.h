/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_BITCRUSHER_H_
#define _DSP_BITCRUSHER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_bitcrusher_config_t config;
    float sample_rate;
    float hold_l;
    float hold_r;
    float phase;
    uint32_t prng_state;
} dsp_bitcrusher_t;

void dsp_bitcrusher_init(dsp_bitcrusher_t *bc, float sample_rate);
void dsp_bitcrusher_set_sample_rate(dsp_bitcrusher_t *bc, float sample_rate);
void dsp_bitcrusher_update_config(dsp_bitcrusher_t *bc, const dsp_bitcrusher_config_t *cfg);
void dsp_bitcrusher_process(dsp_bitcrusher_t *bc, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_BITCRUSHER_H_ */
