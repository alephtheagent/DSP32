/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_ENGINE_H_
#define _DSP_ENGINE_H_

#include <stddef.h>
#include "dsp_types.h"
#include "dsp_eq.h"
#include "dsp_compressor.h"
#include "dsp_limiter.h"
#include "dsp_loudness.h"
#include "dsp_pitch.h"
#include "dsp_deesser.h"
#include "dsp_crossfeed.h"
#include "dsp_stereo_widener.h"
#include "dsp_tape.h"
#include "dsp_bitcrusher.h"
#include "dsp_bass_enhancer.h"
#include "dsp_reverb_delay.h"
#include "dsp_meter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dsp_config_t config;
    float sample_rate;
    float master_vol_linear;

    dsp_eq_t             eq;
    dsp_compressor_t     comp;
    dsp_limiter_t        limiter;
    dsp_loudness_t       loudness;
    dsp_pitch_t          pitch;
    dsp_deesser_t        deesser;
    dsp_crossfeed_t      crossfeed;
    dsp_stereo_widener_t widener;
    dsp_tape_t           tape;
    dsp_bitcrusher_t     bitcrusher;
    dsp_bass_enhancer_t  bass;
    dsp_reverb_delay_t   fx;
    dsp_meter_t          meter;

    SemaphoreHandle_t    lock;
} dsp_engine_t;

void dsp_engine_init(dsp_engine_t *engine, float sample_rate);
void dsp_engine_set_sample_rate(dsp_engine_t *engine, float sample_rate);
void dsp_engine_set_config(dsp_engine_t *engine, const dsp_config_t *config);
void dsp_engine_get_config(dsp_engine_t *engine, dsp_config_t *out_config);
void dsp_engine_set_bit_perfect(dsp_engine_t *engine, bool bypass);
bool dsp_engine_is_bit_perfect(dsp_engine_t *engine);
void dsp_engine_set_master_volume(dsp_engine_t *engine, float volume_db);
void dsp_engine_get_meters(dsp_engine_t *engine, dsp_meter_values_t *out_meters);
void dsp_engine_meter_update_pcm(dsp_engine_t *engine, const uint8_t *pcm, size_t bytes, uint8_t bit_depth);
void dsp_engine_get_waveform(dsp_engine_t *engine, float *out_samples, size_t count);
void dsp_engine_process(dsp_engine_t *engine, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_ENGINE_H_ */
