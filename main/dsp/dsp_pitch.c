/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_pitch.h"
#include <math.h>
#include <string.h>

#define WINDOW_LEN 1024.0f

static void update_pitch_factor(dsp_pitch_t *pitch)
{
    float total_semitones = (float)pitch->config.semitones + (float)pitch->config.cents / 100.0f;
    pitch->pitch_factor = powf(2.0f, total_semitones / 12.0f);
}

void dsp_pitch_init(dsp_pitch_t *pitch, float sample_rate)
{
    pitch->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    pitch->config.enabled = false;
    pitch->config.semitones = 0;
    pitch->config.cents = 0;
    pitch->write_idx = 0;
    pitch->phase = 0.0f;
    pitch->pitch_factor = 1.0f;
    memset(pitch->buf_l, 0, sizeof(pitch->buf_l));
    memset(pitch->buf_r, 0, sizeof(pitch->buf_r));
    update_pitch_factor(pitch);
}

void dsp_pitch_set_sample_rate(dsp_pitch_t *pitch, float sample_rate)
{
    pitch->sample_rate = sample_rate;
}

void dsp_pitch_update_config(dsp_pitch_t *pitch, const dsp_pitch_config_t *config)
{
    pitch->config = *config;
    update_pitch_factor(pitch);
}

void dsp_pitch_process(dsp_pitch_t *pitch, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!pitch->config.enabled) return;
    if (pitch->config.semitones == 0 && pitch->config.cents == 0) return;

    float rate_diff = 1.0f - pitch->pitch_factor;
    float phase = pitch->phase;
    size_t w_idx = pitch->write_idx;

    for (size_t i = 0; i < num_samples; i++) {
        // Write incoming audio to circular buffer
        pitch->buf_l[w_idx] = buf_l[i];
        pitch->buf_r[w_idx] = buf_r[i];

        // Two reading taps separated by 180 degrees (WINDOW_LEN / 2)
        float d1 = phase;
        float d2 = phase + (WINDOW_LEN * 0.5f);
        if (d2 >= WINDOW_LEN) d2 -= WINDOW_LEN;

        // Triangular crossfade window
        float gain1 = 1.0f - fabsf((2.0f * d1 / WINDOW_LEN) - 1.0f);
        float gain2 = 1.0f - gain1;

        // Reading tap 1
        float read_idx1 = (float)w_idx - d1;
        while (read_idx1 < 0.0f) read_idx1 += (float)DSP_PITCH_BUFFER_SIZE;
        size_t r_int1 = (size_t)read_idx1 % DSP_PITCH_BUFFER_SIZE;
        size_t r_next1 = (r_int1 + 1) % DSP_PITCH_BUFFER_SIZE;
        float frac1 = read_idx1 - (float)(size_t)read_idx1;

        float s1_l = pitch->buf_l[r_int1] + frac1 * (pitch->buf_l[r_next1] - pitch->buf_l[r_int1]);
        float s1_r = pitch->buf_r[r_int1] + frac1 * (pitch->buf_r[r_next1] - pitch->buf_r[r_int1]);

        // Reading tap 2
        float read_idx2 = (float)w_idx - d2;
        while (read_idx2 < 0.0f) read_idx2 += (float)DSP_PITCH_BUFFER_SIZE;
        size_t r_int2 = (size_t)read_idx2 % DSP_PITCH_BUFFER_SIZE;
        size_t r_next2 = (r_int2 + 1) % DSP_PITCH_BUFFER_SIZE;
        float frac2 = read_idx2 - (float)(size_t)read_idx2;

        float s2_l = pitch->buf_l[r_int2] + frac2 * (pitch->buf_l[r_next2] - pitch->buf_l[r_int2]);
        float s2_r = pitch->buf_r[r_int2] + frac2 * (pitch->buf_r[r_next2] - pitch->buf_r[r_int2]);

        // Blend output
        buf_l[i] = s1_l * gain1 + s2_l * gain2;
        buf_r[i] = s1_r * gain1 + s2_r * gain2;

        // Advance phase & write pointer
        phase += rate_diff;
        while (phase >= WINDOW_LEN) phase -= WINDOW_LEN;
        while (phase < 0.0f) phase += WINDOW_LEN;

        w_idx = (w_idx + 1) % DSP_PITCH_BUFFER_SIZE;
    }

    pitch->phase = phase;
    pitch->write_idx = w_idx;
}
