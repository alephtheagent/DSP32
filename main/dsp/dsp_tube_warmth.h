/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_TUBE_WARMTH_H_
#define _DSP_TUBE_WARMTH_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_tube_config_t config;
    float dc_x_l, dc_y_l;
    float dc_x_r, dc_y_r;
} dsp_tube_warmth_t;

void dsp_tube_warmth_init(dsp_tube_warmth_t *tube);
void dsp_tube_warmth_update_config(dsp_tube_warmth_t *tube, const dsp_tube_config_t *config);
void dsp_tube_warmth_process(dsp_tube_warmth_t *tube, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_TUBE_WARMTH_H_ */
