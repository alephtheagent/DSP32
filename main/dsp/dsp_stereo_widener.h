/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_STEREO_WIDENER_H_
#define _DSP_STEREO_WIDENER_H_

#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_widener_config_t config;
} dsp_stereo_widener_t;

void dsp_stereo_widener_init(dsp_stereo_widener_t *widener);
void dsp_stereo_widener_update_config(dsp_stereo_widener_t *widener, const dsp_widener_config_t *config);
void dsp_stereo_widener_process(dsp_stereo_widener_t *widener, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_STEREO_WIDENER_H_ */
