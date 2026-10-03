/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_TAPE_H_
#define _DSP_TAPE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_TAPE_DELAY_BUF_SIZE 2048

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float z1, z2;
} dsp_tape_biquad_t;

typedef struct {
    dsp_tape_config_t config;
    uint32_t sample_rate;

    // Filters for Head Bump (Low-end resonance)
    dsp_tape_biquad_t head_bump_l;
    dsp_tape_biquad_t head_bump_r;

    // Filters for Lo-Fi HF Cutoff (Gap loss)
    dsp_tape_biquad_t hf_cut_l;
    dsp_tape_biquad_t hf_cut_r;

    // Tape Hiss Pink Noise generator & post-processing filters
    uint32_t noise_state_l;
    uint32_t noise_state_r;
    float pink_b0_l, pink_b1_l, pink_b2_l;
    float pink_b0_r, pink_b1_r, pink_b2_r;
    dsp_tape_biquad_t hiss_lp_l;
    dsp_tape_biquad_t hiss_lp_r;
    dsp_tape_biquad_t hiss_hp_l;
    dsp_tape_biquad_t hiss_hp_r;

    // Soft Tape Clicks / Pops state (dual-stage smooth low-pass)
    int click_samples_left;
    int click_total_samples;
    float click_amp_l;
    float click_amp_r;
    float click_lp1_l, click_lp2_l;
    float click_lp1_r, click_lp2_r;
    uint32_t click_prng;

    // Wow & Flutter (mechanical delay modulation)
    float delay_buf_l[DSP_TAPE_DELAY_BUF_SIZE];
    float delay_buf_r[DSP_TAPE_DELAY_BUF_SIZE];
    size_t delay_write_pos;
    float lfo_phase_wow;
    float lfo_phase_flutter;

    // DC Blocker
    float dc_x_l, dc_y_l;
    float dc_x_r, dc_y_r;
} dsp_tape_t;

void dsp_tape_init(dsp_tape_t *tape, uint32_t sample_rate);
void dsp_tape_update_config(dsp_tape_t *tape, const dsp_tape_config_t *config, uint32_t sample_rate);
void dsp_tape_process(dsp_tape_t *tape, float *buf_l, float *buf_r, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_TAPE_H_ */
