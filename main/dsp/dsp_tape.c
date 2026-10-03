/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_tape.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Fast 32-bit PRNG (XorShift)
static inline uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0) x = 0x12345678;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline float prng_to_float(uint32_t x)
{
    return ((int32_t)x) * (1.0f / 2147483648.0f);
}

// Biquad calculation helpers
static void biquad_calc_peaking(dsp_tape_biquad_t *bq, float f0, float gain_db, float q, float fs)
{
    if (fabsf(gain_db) < 0.05f) {
        bq->b0 = 1.0f; bq->b1 = 0.0f; bq->b2 = 0.0f;
        bq->a1 = 0.0f; bq->a2 = 0.0f;
        return;
    }
    float A = powf(10.0f, gain_db / 40.0f);
    float w0 = 2.0f * (float)M_PI * f0 / fs;
    float alpha = sinf(w0) / (2.0f * q);
    float cos_w0 = cosf(w0);

    float a0 = 1.0f + alpha / A;
    bq->b0 = (1.0f + alpha * A) / a0;
    bq->b1 = (-2.0f * cos_w0) / a0;
    bq->b2 = (1.0f - alpha * A) / a0;
    bq->a1 = (-2.0f * cos_w0) / a0;
    bq->a2 = (1.0f - alpha / A) / a0;
}

static void biquad_calc_lowpass(dsp_tape_biquad_t *bq, float fc, float q, float fs)
{
    if (fc >= fs * 0.48f) {
        // Transparent pass-through
        bq->b0 = 1.0f; bq->b1 = 0.0f; bq->b2 = 0.0f;
        bq->a1 = 0.0f; bq->a2 = 0.0f;
        return;
    }
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float alpha = sinf(w0) / (2.0f * q);
    float cos_w0 = cosf(w0);

    float a0 = 1.0f + alpha;
    bq->b0 = ((1.0f - cos_w0) * 0.5f) / a0;
    bq->b1 = (1.0f - cos_w0) / a0;
    bq->b2 = ((1.0f - cos_w0) * 0.5f) / a0;
    bq->a1 = (-2.0f * cos_w0) / a0;
    bq->a2 = (1.0f - alpha) / a0;
}

static void biquad_calc_highpass(dsp_tape_biquad_t *bq, float fc, float q, float fs)
{
    if (fc <= 10.0f) {
        // Transparent pass-through
        bq->b0 = 1.0f; bq->b1 = 0.0f; bq->b2 = 0.0f;
        bq->a1 = 0.0f; bq->a2 = 0.0f;
        return;
    }
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float alpha = sinf(w0) / (2.0f * q);
    float cos_w0 = cosf(w0);

    float a0 = 1.0f + alpha;
    bq->b0 = ((1.0f + cos_w0) * 0.5f) / a0;
    bq->b1 = -(1.0f + cos_w0) / a0;
    bq->b2 = ((1.0f + cos_w0) * 0.5f) / a0;
    bq->a1 = (-2.0f * cos_w0) / a0;
    bq->a2 = (1.0f - alpha) / a0;
}

// Paul Kellet's Pink Noise IIR Filter (-3dB/oct continuous spectrum)
static inline float generate_pink_noise(uint32_t *prng_state, float *b0, float *b1, float *b2)
{
    float white = prng_to_float(xorshift32(prng_state));
    *b0 = 0.99765f * (*b0) + white * 0.0990460f;
    *b1 = 0.96300f * (*b1) + white * 0.2965164f;
    *b2 = 0.57000f * (*b2) + white * 1.0526913f;

    // Flush denormals
    if (fabsf(*b0) < 1e-15f) *b0 = 0.0f;
    if (fabsf(*b1) < 1e-15f) *b1 = 0.0f;
    if (fabsf(*b2) < 1e-15f) *b2 = 0.0f;

    float pink = *b0 + *b1 + *b2 + white * 0.1848f;
    return pink * 0.12f; // Scaled to ~ unity RMS
}

static inline float biquad_process(dsp_tape_biquad_t *bq, float in)
{
    float out = bq->b0 * in + bq->z1;
    bq->z1 = bq->b1 * in - bq->a1 * out + bq->z2;
    bq->z2 = bq->b2 * in - bq->a2 * out;

    // Flush denormals
    if (fabsf(bq->z1) < 1e-15f) bq->z1 = 0.0f;
    if (fabsf(bq->z2) < 1e-15f) bq->z2 = 0.0f;

    return out;
}

void dsp_tape_init(dsp_tape_t *tape, uint32_t sample_rate)
{
    memset(tape, 0, sizeof(dsp_tape_t));
    tape->sample_rate = sample_rate ? sample_rate : 48000;

    // Default configuration: subtle, musical, warm vintage cassette
    tape->config.enabled = false;
    tape->config.drive = 0.20f;
    tape->config.hf_cut_hz = 15000.0f;
    tape->config.head_bump = 0.25f;
    tape->config.hiss_level = 0.01f;
    tape->config.click_rate = 0.05f;
    tape->config.click_level = 0.03f;
    tape->config.wow_flutter = 0.10f;

    tape->noise_state_l = 0xA5A5A5A5;
    tape->noise_state_r = 0x5A5A5A5A;
    tape->click_prng = 0x1337BEEF;

    dsp_tape_update_config(tape, &tape->config, tape->sample_rate);
}

void dsp_tape_update_config(dsp_tape_t *tape, const dsp_tape_config_t *config, uint32_t sample_rate)
{
    tape->config = *config;
    if (sample_rate) tape->sample_rate = sample_rate;
    float fs = (float)tape->sample_rate;

    // Clamp parameters
    if (tape->config.drive < 0.0f) tape->config.drive = 0.0f;
    if (tape->config.drive > 1.0f) tape->config.drive = 1.0f;

    if (tape->config.hf_cut_hz < 3000.0f) tape->config.hf_cut_hz = 3000.0f;
    if (tape->config.hf_cut_hz > 22000.0f) tape->config.hf_cut_hz = 22000.0f;

    if (tape->config.head_bump < 0.0f) tape->config.head_bump = 0.0f;
    if (tape->config.head_bump > 1.0f) tape->config.head_bump = 1.0f;

    if (tape->config.hiss_level < 0.0f) tape->config.hiss_level = 0.0f;
    if (tape->config.hiss_level > 1.0f) tape->config.hiss_level = 1.0f;

    if (tape->config.click_rate < 0.0f) tape->config.click_rate = 0.0f;
    if (tape->config.click_rate > 1.0f) tape->config.click_rate = 1.0f;

    if (tape->config.click_level < 0.0f) tape->config.click_level = 0.0f;
    if (tape->config.click_level > 1.0f) tape->config.click_level = 1.0f;

    if (tape->config.wow_flutter < 0.0f) tape->config.wow_flutter = 0.0f;
    if (tape->config.wow_flutter > 1.0f) tape->config.wow_flutter = 1.0f;

    // 1. Head Bump filter: Peaking at ~65 Hz, gain 0 to +4.5 dB, Q = 1.2
    float head_bump_gain = tape->config.head_bump * 4.5f;
    biquad_calc_peaking(&tape->head_bump_l, 65.0f, head_bump_gain, 1.2f, fs);
    biquad_calc_peaking(&tape->head_bump_r, 65.0f, head_bump_gain, 1.2f, fs);

    // 2. Lo-Fi HF Cutoff: Butterworth Low-Pass (Q = 0.707)
    biquad_calc_lowpass(&tape->hf_cut_l, tape->config.hf_cut_hz, 0.7071f, fs);
    biquad_calc_lowpass(&tape->hf_cut_r, tape->config.hf_cut_hz, 0.7071f, fs);

    // 3. Tape Hiss Post-Processing Filters:
    // High-pass at 350 Hz (removes sub-bass rumble)
    biquad_calc_highpass(&tape->hiss_hp_l, 350.0f, 0.7071f, fs);
    biquad_calc_highpass(&tape->hiss_hp_r, 350.0f, 0.7071f, fs);

    // Low-pass at 6500 Hz (removes harsh static fizz)
    biquad_calc_lowpass(&tape->hiss_lp_l, 6500.0f, 0.7071f, fs);
    biquad_calc_lowpass(&tape->hiss_lp_r, 6500.0f, 0.7071f, fs);
}

// Magnetic Tape Saturation with Asymmetric Bias & Soft Compression
static inline float tape_saturate_sample(float in, float drive)
{
    // Input drive boost (1.0 to 3.5x)
    float x = in * (1.0f + drive * 2.5f);

    // Asymmetric bias: introduces gentle warm 2nd harmonic along with dominant 3rd
    float bias = 0.16f * drive * (x * fabsf(x));
    float xb = x + bias;

    // Rational soft saturation curve (analog tape hysteresis approximation)
    // y = xb / (1 + 0.65*|xb| + 0.25*xb^2)
    float abs_xb = fabsf(xb);
    float y = xb / (1.0f + 0.65f * abs_xb + 0.25f * xb * xb);

    // Automatic makeup gain compensation: thickens sound without extreme volume jump
    float norm = (1.0f + drive * 0.35f) / (1.0f + drive * 1.35f);
    return y * norm;
}

void dsp_tape_process(dsp_tape_t *tape, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!tape->config.enabled) return;

    float drive = tape->config.drive;
    float hiss_vol = tape->config.hiss_level * 0.008f; // ~ -72 dBFS at 1%
    float click_rate = tape->config.click_rate;
    float click_level = tape->config.click_level;
    float wow_flutter = tape->config.wow_flutter;
    float fs = (float)tape->sample_rate;

    const float dc_pole = 0.995f; // DC-blocker pole (~15Hz at 48k)
    float dc_xl = tape->dc_x_l, dc_yl = tape->dc_y_l;
    float dc_xr = tape->dc_x_r, dc_yr = tape->dc_y_r;

    // LFO phase increments
    // Wow: ~0.85 Hz slow capstan drift
    float wow_inc = (2.0f * (float)M_PI * 0.85f) / fs;
    // Flutter: ~6.8 Hz mechanical flutter
    float flutter_inc = (2.0f * (float)M_PI * 6.8f) / fs;

    float lfo_wow = tape->lfo_phase_wow;
    float lfo_flutter = tape->lfo_phase_flutter;

    // Soft click Poisson trigger threshold per sample
    uint32_t click_thresh = (uint32_t)(click_rate * 360.0f);

    size_t wpos = tape->delay_write_pos;

    // Wow & Flutter modulation depths (Doppler pitch deviation)
    float wow_amp = wow_flutter * 220.0f;
    float flutter_amp = wow_flutter * 32.0f;

    for (size_t i = 0; i < num_samples; i++) {
        float in_l = buf_l[i];
        float in_r = buf_r[i];

        // 1. Magnetic Tape Saturation & Warmth
        float sat_l = tape_saturate_sample(in_l, drive);
        float sat_r = tape_saturate_sample(in_r, drive);

        // 2. Frequency Contour: Head Bump (65 Hz resonance)
        float bump_l = biquad_process(&tape->head_bump_l, sat_l);
        float bump_r = biquad_process(&tape->head_bump_r, sat_r);

        // 3. Lo-Fi Degradation: HF Cutoff (Gap loss)
        float hf_l = biquad_process(&tape->hf_cut_l, bump_l);
        float hf_r = biquad_process(&tape->hf_cut_r, bump_r);

        // 4. Wow & Flutter (Delay line pitch modulation)
        // Continuously write into ring buffer
        tape->delay_buf_l[wpos] = hf_l;
        tape->delay_buf_r[wpos] = hf_r;

        float out_l = hf_l;
        float out_r = hf_r;

        if (wow_flutter > 0.001f) {
            // Distinct stereo phase offset for wide analog tape dimension
            float mod_l = wow_amp * sinf(lfo_wow) + flutter_amp * sinf(lfo_flutter);
            float mod_r = wow_amp * sinf(lfo_wow + 0.35f) + flutter_amp * sinf(lfo_flutter + 0.25f);

            float delay_l = 1024.0f + mod_l;
            float delay_r = 1024.0f + mod_r;

            if (delay_l < 8.0f) delay_l = 8.0f;
            if (delay_l > (float)(DSP_TAPE_DELAY_BUF_SIZE - 8)) delay_l = (float)(DSP_TAPE_DELAY_BUF_SIZE - 8);

            if (delay_r < 8.0f) delay_r = 8.0f;
            if (delay_r > (float)(DSP_TAPE_DELAY_BUF_SIZE - 8)) delay_r = (float)(DSP_TAPE_DELAY_BUF_SIZE - 8);

            // Left channel fractional delay interpolation
            float rpos_l = (float)wpos - delay_l;
            if (rpos_l < 0.0f) rpos_l += (float)DSP_TAPE_DELAY_BUF_SIZE;
            size_t idx0_l = ((size_t)rpos_l) & (DSP_TAPE_DELAY_BUF_SIZE - 1);
            size_t idx1_l = (idx0_l + 1) & (DSP_TAPE_DELAY_BUF_SIZE - 1);
            float frac_l = rpos_l - (float)((size_t)rpos_l);
            out_l = tape->delay_buf_l[idx0_l] + frac_l * (tape->delay_buf_l[idx1_l] - tape->delay_buf_l[idx0_l]);

            // Right channel fractional delay interpolation
            float rpos_r = (float)wpos - delay_r;
            if (rpos_r < 0.0f) rpos_r += (float)DSP_TAPE_DELAY_BUF_SIZE;
            size_t idx0_r = ((size_t)rpos_r) & (DSP_TAPE_DELAY_BUF_SIZE - 1);
            size_t idx1_r = (idx0_r + 1) & (DSP_TAPE_DELAY_BUF_SIZE - 1);
            float frac_r = rpos_r - (float)((size_t)rpos_r);
            out_r = tape->delay_buf_r[idx0_r] + frac_r * (tape->delay_buf_r[idx1_r] - tape->delay_buf_r[idx0_r]);

            lfo_wow += wow_inc;
            if (lfo_wow > 2.0f * (float)M_PI) lfo_wow -= 2.0f * (float)M_PI;

            lfo_flutter += flutter_inc;
            if (lfo_flutter > 2.0f * (float)M_PI) lfo_flutter -= 2.0f * (float)M_PI;
        }

        wpos = (wpos + 1) & (DSP_TAPE_DELAY_BUF_SIZE - 1);

        // 5. Tape Hiss (Filtered Pink Noise with HP 350Hz + LP 6.5kHz)
        if (hiss_vol > 1e-6f) {
            float raw_pink_l = generate_pink_noise(&tape->noise_state_l, &tape->pink_b0_l, &tape->pink_b1_l, &tape->pink_b2_l);
            float raw_pink_r = generate_pink_noise(&tape->noise_state_r, &tape->pink_b0_r, &tape->pink_b1_r, &tape->pink_b2_r);

            float hp_l = biquad_process(&tape->hiss_hp_l, raw_pink_l);
            float hp_r = biquad_process(&tape->hiss_hp_r, raw_pink_r);

            float hiss_l = biquad_process(&tape->hiss_lp_l, hp_l) * hiss_vol;
            float hiss_r = biquad_process(&tape->hiss_lp_r, hp_r) * hiss_vol;

            out_l += hiss_l;
            out_r += hiss_r;
        }

        // 6. Random Soft Tape Clicks / Pops (Organic 450Hz low-passed micro-pops)
        if (click_rate > 0.001f && click_level > 0.001f) {
            // Check for new random click trigger
            if (tape->click_samples_left <= 0) {
                uint32_t r = xorshift32(&tape->click_prng) & 0x7FFFFFFF;
                if ((r % 48000) < click_thresh) {
                    // Trigger new soft click (~5 to 7.5 ms duration)
                    tape->click_total_samples = 220 + (r % 120);
                    tape->click_samples_left = tape->click_total_samples;

                    // Subtle amplitude and organic pan
                    float base_amp = (0.005f + 0.008f * prng_to_float(xorshift32(&tape->click_prng))) * click_level;
                    float pan = 0.35f + 0.30f * fabsf(prng_to_float(xorshift32(&tape->click_prng)));

                    tape->click_amp_l = base_amp * pan;
                    tape->click_amp_r = base_amp * (1.0f - pan);
                }
            }

            // Synthesize ongoing click impulse
            if (tape->click_samples_left > 0) {
                float progress = 1.0f - ((float)tape->click_samples_left / (float)tape->click_total_samples);
                // Ultra-smooth sin^2(pi * t) envelope
                float s = sinf(progress * (float)M_PI);
                float pulse = s * s;

                // 2-stage cascaded low-pass filter at ~450 Hz for ultra-soft muffled analog pop
                const float alpha_lp = 0.055f;
                tape->click_lp1_l += alpha_lp * (pulse * tape->click_amp_l - tape->click_lp1_l);
                tape->click_lp1_r += alpha_lp * (pulse * tape->click_amp_r - tape->click_lp1_r);

                tape->click_lp2_l += alpha_lp * (tape->click_lp1_l - tape->click_lp2_l);
                tape->click_lp2_r += alpha_lp * (tape->click_lp1_r - tape->click_lp2_r);

                out_l += tape->click_lp2_l;
                out_r += tape->click_lp2_r;

                tape->click_samples_left--;
            } else {
                if (fabsf(tape->click_lp2_l) > 1e-7f) {
                    tape->click_lp1_l *= 0.95f; tape->click_lp2_l *= 0.95f;
                    tape->click_lp1_r *= 0.95f; tape->click_lp2_r *= 0.95f;
                    out_l += tape->click_lp2_l;
                    out_r += tape->click_lp2_r;
                } else {
                    tape->click_lp1_l = tape->click_lp2_l = 0.0f;
                    tape->click_lp1_r = tape->click_lp2_r = 0.0f;
                }
            }
        }

        // 7. DC-Blocking Filter: y[n] = x[n] - x[n-1] + R * y[n-1]
        float yl = out_l - dc_xl + dc_pole * dc_yl;
        dc_xl = out_l;
        dc_yl = yl;

        float yr = out_r - dc_xr + dc_pole * dc_yr;
        dc_xr = out_r;
        dc_yr = yr;

        buf_l[i] = yl;
        buf_r[i] = yr;
    }

    // Flush denormals
    if (fabsf(dc_yl) < 1e-15f) dc_yl = 0.0f;
    if (fabsf(dc_yr) < 1e-15f) dc_yr = 0.0f;

    tape->dc_x_l = dc_xl; tape->dc_y_l = dc_yl;
    tape->dc_x_r = dc_xr; tape->dc_y_r = dc_yr;
    tape->delay_write_pos = wpos;
    tape->lfo_phase_wow = lfo_wow;
    tape->lfo_phase_flutter = lfo_flutter;
}
