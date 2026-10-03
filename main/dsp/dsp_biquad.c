/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_biquad.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void dsp_biquad_reset(dsp_biquad_t *f)
{
    f->s1_l = f->s2_l = 0.0f;
    f->s1_r = f->s2_r = 0.0f;
    f->b0 = 1.0f; f->b1 = 0.0f; f->b2 = 0.0f;
    f->a1 = 0.0f; f->a2 = 0.0f;
    f->target_b0 = 1.0f; f->target_b1 = 0.0f; f->target_b2 = 0.0f;
    f->target_a1 = 0.0f; f->target_a2 = 0.0f;
    f->interpolating = false;
}

void dsp_biquad_calc(dsp_biquad_t *f, dsp_filter_type_t type, float sample_rate, float freq, float gain_db, float q, bool immediate)
{
    if (sample_rate <= 0.0f) sample_rate = 48000.0f;
    if (freq < 10.0f) freq = 10.0f;
    if (freq > sample_rate * 0.49f) freq = sample_rate * 0.49f;
    if (q < 0.1f) q = 0.1f;
    if (q > 15.0f) q = 15.0f;

    float omega = 2.0f * (float)M_PI * freq / sample_rate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * q);
    float A = powf(10.0f, gain_db / 40.0f);

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

    switch (type) {
    case DSP_FILTER_LOW_SHELF: {
        float sqrtA = sqrtf(A);
        float two_sqrtA_alpha = 2.0f * sqrtA * alpha;
        b0 =    A * ((A + 1.0f) - (A - 1.0f) * cos_omega + two_sqrtA_alpha);
        b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_omega);
        b2 =    A * ((A + 1.0f) - (A - 1.0f) * cos_omega - two_sqrtA_alpha);
        a0 =         (A + 1.0f) + (A - 1.0f) * cos_omega + two_sqrtA_alpha;
        a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_omega);
        a2 =         (A + 1.0f) + (A - 1.0f) * cos_omega - two_sqrtA_alpha;
        break;
    }
    case DSP_FILTER_HIGH_SHELF: {
        float sqrtA = sqrtf(A);
        float two_sqrtA_alpha = 2.0f * sqrtA * alpha;
        b0 =    A * ((A + 1.0f) + (A - 1.0f) * cos_omega + two_sqrtA_alpha);
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_omega);
        b2 =    A * ((A + 1.0f) + (A - 1.0f) * cos_omega - two_sqrtA_alpha);
        a0 =         (A + 1.0f) - (A - 1.0f) * cos_omega + two_sqrtA_alpha;
        a1 =  2.0f * ((A - 1.0f) - (A + 1.0f) * cos_omega);
        a2 =         (A + 1.0f) - (A - 1.0f) * cos_omega - two_sqrtA_alpha;
        break;
    }
    case DSP_FILTER_PEAKING: {
        b0 = 1.0f + alpha * A;
        b1 = -2.0f * cos_omega;
        b2 = 1.0f - alpha * A;
        a0 = 1.0f + alpha / A;
        a1 = -2.0f * cos_omega;
        a2 = 1.0f - alpha / A;
        break;
    }
    case DSP_FILTER_LOW_PASS: {
        b0 = (1.0f - cos_omega) * 0.5f;
        b1 = 1.0f - cos_omega;
        b2 = (1.0f - cos_omega) * 0.5f;
        a0 = 1.0f + alpha;
        a1 = -2.0f * cos_omega;
        a2 = 1.0f - alpha;
        break;
    }
    case DSP_FILTER_HIGH_PASS: {
        b0 = (1.0f + cos_omega) * 0.5f;
        b1 = -(1.0f + cos_omega);
        b2 = (1.0f + cos_omega) * 0.5f;
        a0 = 1.0f + alpha;
        a1 = -2.0f * cos_omega;
        a2 = 1.0f - alpha;
        break;
    }
    case DSP_FILTER_BAND_PASS: {
        b0 = alpha;
        b1 = 0.0f;
        b2 = -alpha;
        a0 = 1.0f + alpha;
        a1 = -2.0f * cos_omega;
        a2 = 1.0f - alpha;
        break;
    }
    }

    float inv_a0 = 1.0f / a0;
    float norm_b0 = b0 * inv_a0;
    float norm_b1 = b1 * inv_a0;
    float norm_b2 = b2 * inv_a0;
    float norm_a1 = a1 * inv_a0;
    float norm_a2 = a2 * inv_a0;

    if (immediate) {
        f->b0 = norm_b0; f->b1 = norm_b1; f->b2 = norm_b2;
        f->a1 = norm_a1; f->a2 = norm_a2;
        f->target_b0 = norm_b0; f->target_b1 = norm_b1; f->target_b2 = norm_b2;
        f->target_a1 = norm_a1; f->target_a2 = norm_a2;
        f->interpolating = false;
    } else {
        f->target_b0 = norm_b0; f->target_b1 = norm_b1; f->target_b2 = norm_b2;
        f->target_a1 = norm_a1; f->target_a2 = norm_a2;
        f->interpolating = true;
    }
}

void dsp_biquad_process_stereo(dsp_biquad_t *f, const float *in_l, const float *in_r, float *out_l, float *out_r, size_t num_samples)
{
    if (num_samples == 0) return;

    float b0 = f->b0, b1 = f->b1, b2 = f->b2;
    float a1 = f->a1, a2 = f->a2;
    float s1_l = f->s1_l, s2_l = f->s2_l;
    float s1_r = f->s1_r, s2_r = f->s2_r;

    // Smooth parameter interpolation over the sample block
    float step = f->interpolating ? (1.0f / (float)num_samples) : 0.0f;
    float db0 = (f->target_b0 - b0) * step;
    float db1 = (f->target_b1 - b1) * step;
    float db2 = (f->target_b2 - b2) * step;
    float da1 = (f->target_a1 - a1) * step;
    float da2 = (f->target_a2 - a2) * step;

    for (size_t i = 0; i < num_samples; i++) {
        if (f->interpolating) {
            b0 += db0; b1 += db1; b2 += db2;
            a1 += da1; a2 += da2;
        }

        // Left channel (Direct Form II Transposed)
        float xl = in_l[i];
        float yl = b0 * xl + s1_l;
        s1_l = b1 * xl - a1 * yl + s2_l;
        s2_l = b2 * xl - a2 * yl;
        out_l[i] = yl;

        // Right channel
        float xr = in_r[i];
        float yr = b0 * xr + s1_r;
        s1_r = b1 * xr - a1 * yr + s2_r;
        s2_r = b2 * xr - a2 * yr;
        out_r[i] = yr;
    }

    if (f->interpolating) {
        f->b0 = f->target_b0; f->b1 = f->target_b1; f->b2 = f->target_b2;
        f->a1 = f->target_a1; f->a2 = f->target_a2;
        f->interpolating = false;
    } else {
        f->b0 = b0; f->b1 = b1; f->b2 = b2;
        f->a1 = a1; f->a2 = a2;
    }

    // Flush denormals to prevent CPU slowdowns
    if (fabsf(s1_l) < 1e-15f) s1_l = 0.0f;
    if (fabsf(s2_l) < 1e-15f) s2_l = 0.0f;
    if (fabsf(s1_r) < 1e-15f) s1_r = 0.0f;
    if (fabsf(s2_r) < 1e-15f) s2_r = 0.0f;

    f->s1_l = s1_l; f->s2_l = s2_l;
    f->s1_r = s1_r; f->s2_r = s2_r;
}
