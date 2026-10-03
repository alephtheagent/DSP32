/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _AUDIO_PIPELINE_H_
#define _AUDIO_PIPELINE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "dsp/dsp_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LATENCY_MODE_ULTRA_LOW_16  = 16,
    LATENCY_MODE_ULTRA_LOW_32  = 32,
    LATENCY_MODE_LOW_64        = 64,
    LATENCY_MODE_SAFE_128      = 128,
    LATENCY_MODE_SAFE_256      = 256,
} audio_buffer_size_t;

typedef struct {
    audio_buffer_size_t buffer_size_samples;
    uint32_t sample_rate;
    uint8_t bit_depth;
    bool bit_perfect;
    size_t ringbuf_fill_bytes;
    size_t ringbuf_total_bytes;
    uint32_t underrun_count;
    uint32_t overrun_count;
} audio_pipeline_stats_t;

esp_err_t audio_pipeline_init(uint32_t sample_rate, uint8_t bit_depth);
void audio_pipeline_write_usb_data(const uint8_t *data, size_t len, uint8_t bit_depth);
void audio_pipeline_set_format(uint32_t sample_rate, uint8_t bit_depth);
void audio_pipeline_set_buffer_size(audio_buffer_size_t size);
audio_buffer_size_t audio_pipeline_get_buffer_size(void);
void audio_pipeline_get_stats(audio_pipeline_stats_t *out_stats);
dsp_engine_t *audio_pipeline_get_dsp_engine(void);

#ifdef __cplusplus
}
#endif

#endif /* _AUDIO_PIPELINE_H_ */
