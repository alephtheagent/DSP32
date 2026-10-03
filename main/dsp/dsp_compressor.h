/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_COMPRESSOR_H_
#define _DSP_COMPRESSOR_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_compressor_config_t config;
    float sample_rate;
    float env_db;
    float alpha_attack;
    float alpha_release;
    float makeup_linear;
} dsp_compressor_t;

void dsp_compressor_init(dsp_compressor_t *comp, float sample_rate);
void dsp_compressor_set_sample_rate(dsp_compressor_t *comp, float sample_rate);
void dsp_compressor_update_config(dsp_compressor_t *comp, const dsp_compressor_config_t *config);
void dsp_compressor_process(dsp_compressor_t *comp, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_COMPRESSOR_H_ */
