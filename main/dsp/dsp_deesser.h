/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_DEESSER_H_
#define _DSP_DEESSER_H_

#include <stddef.h>
#include "dsp_types.h"
#include "dsp_biquad.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_deesser_config_t config;
    dsp_biquad_t sidechain_bp;
    float sample_rate;
    float env_db;
    float alpha_attack;
    float alpha_release;
} dsp_deesser_t;

void dsp_deesser_init(dsp_deesser_t *deesser, float sample_rate);
void dsp_deesser_set_sample_rate(dsp_deesser_t *deesser, float sample_rate);
void dsp_deesser_update_config(dsp_deesser_t *deesser, const dsp_deesser_config_t *config);
void dsp_deesser_process(dsp_deesser_t *deesser, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_DEESSER_H_ */
