/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_bitcrusher.h"
#include <math.h>
#include <string.h>

static inline float get_tpdf_dither(uint32_t *state)
{
    // 32-bit Linear Congruential Generator
    *state = *state * 1664525u + 1013904223u;
    float r1 = (float)(*state >> 16) * (1.0f / 65536.0f);
    *state = *state * 1664525u + 1013904223u;
    float r2 = (float)(*state >> 16) * (1.0f / 65536.0f);
    return r1 - r2; // [-1.0, +1.0] triangular probability density function
}

void dsp_bitcrusher_init(dsp_bitcrusher_t *bc, float sample_rate)
{
    memset(bc, 0, sizeof(dsp_bitcrusher_t));
    bc->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    bc->config.enabled = false;
    bc->config.enable_downsample = true;
    bc->config.enable_quantize = true;
    bc->config.enable_overflow = false;
    bc->config.dither = false;
    bc->config.bit_depth = 8;
    bc->config.downsample_rate = 8000.0f;
    bc->config.overflow_intensity = 0.3f;
    bc->config.mix = 1.0f;

    bc->hold_l = 0.0f;
    bc->hold_r = 0.0f;
    bc->phase = 1.0f; // trigger immediate sample on first block
    bc->prng_state = 0x12345678;
}

void dsp_bitcrusher_set_sample_rate(dsp_bitcrusher_t *bc, float sample_rate)
{
    if (sample_rate > 0) {
        bc->sample_rate = sample_rate;
    }
}

void dsp_bitcrusher_update_config(dsp_bitcrusher_t *bc, const dsp_bitcrusher_config_t *cfg)
{
    if (!cfg) return;
    bc->config = *cfg;

    // Parameter sanitization
    if (bc->config.bit_depth < 2) bc->config.bit_depth = 2;
    if (bc->config.bit_depth > 16) bc->config.bit_depth = 16;

    if (bc->config.downsample_rate < 200.0f) bc->config.downsample_rate = 200.0f;
    if (bc->config.downsample_rate > bc->sample_rate) bc->config.downsample_rate = bc->sample_rate;

    if (bc->config.overflow_intensity < 0.0f) bc->config.overflow_intensity = 0.0f;
    if (bc->config.overflow_intensity > 1.0f) bc->config.overflow_intensity = 1.0f;

    if (bc->config.mix < 0.0f) bc->config.mix = 0.0f;
    if (bc->config.mix > 1.0f) bc->config.mix = 1.0f;
}

void dsp_bitcrusher_process(dsp_bitcrusher_t *bc, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!bc->config.enabled || bc->config.mix <= 0.0001f || num_samples == 0) {
        return;
    }

    const bool do_downsample = bc->config.enable_downsample;
    const bool do_quantize   = bc->config.enable_quantize;
    const bool do_overflow   = bc->config.enable_overflow;
    const bool do_dither     = bc->config.dither;
    const float mix          = bc->config.mix;

    // Downsampling phase step
    float phase_step = bc->config.downsample_rate / bc->sample_rate;
    if (phase_step > 1.0f) phase_step = 1.0f;
    if (phase_step < 0.0001f) phase_step = 0.0001f;

    // Quantization steps: 2^bits levels
    const float q_levels = (float)(1 << bc->config.bit_depth);
    const float half_levels = q_levels * 0.5f;
    const float inv_half_levels = 1.0f / half_levels;
    const float dither_scale = 1.0f / q_levels;

    // Overflow drive multiplier: 1.0 to 6.0
    const float overflow_drive = 1.0f + bc->config.overflow_intensity * 5.0f;

    for (size_t i = 0; i < num_samples; i++) {
        const float dry_l = buf_l[i];
        const float dry_r = buf_r[i];
        float wet_l = dry_l;
        float wet_r = dry_r;

        // 1. Sample Rate Reduction (Sample & Hold)
        if (do_downsample) {
            bc->phase += phase_step;
            if (bc->phase >= 1.0f) {
                bc->phase -= 1.0f;
                if (bc->phase >= 1.0f) bc->phase = 0.0f;
                bc->hold_l = dry_l;
                bc->hold_r = dry_r;
            }
            wet_l = bc->hold_l;
            wet_r = bc->hold_r;
        }

        // 2. Bit Resolution Quantization
        if (do_quantize) {
            float sl = wet_l;
            float sr = wet_r;

            if (do_dither) {
                sl += get_tpdf_dither(&bc->prng_state) * dither_scale;
                sr += get_tpdf_dither(&bc->prng_state) * dither_scale;
            }

            sl = roundf(sl * half_levels) * inv_half_levels;
            sr = roundf(sr * half_levels) * inv_half_levels;

            if (sl > 1.0f) sl = 1.0f;
            else if (sl < -1.0f) sl = -1.0f;

            if (sr > 1.0f) sr = 1.0f;
            else if (sr < -1.0f) sr = -1.0f;

            wet_l = sl;
            wet_r = sr;
        }

        // 3. Bit Overflow / 2's Complement Modulo Wrap
        if (do_overflow) {
            // Scale and wrap into [-1.0, 1.0] range
            float ol = wet_l * overflow_drive + 1.0f;
            float wl = fmodf(ol, 2.0f);
            if (wl < 0.0f) wl += 2.0f;
            wet_l = wl - 1.0f;

            float or = wet_r * overflow_drive + 1.0f;
            float wr = fmodf(or, 2.0f);
            if (wr < 0.0f) wr += 2.0f;
            wet_r = wr - 1.0f;
        }

        // 4. Wet / Dry Mix Blend
        buf_l[i] = dry_l * (1.0f - mix) + wet_l * mix;
        buf_r[i] = dry_r * (1.0f - mix) + wet_r * mix;
    }
}
