/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _DSP_TYPES_H_
#define _DSP_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_EQ_NUM_BANDS 10

typedef enum {
    DSP_FILTER_LOW_SHELF = 0,
    DSP_FILTER_PEAKING,
    DSP_FILTER_HIGH_SHELF,
    DSP_FILTER_LOW_PASS,
    DSP_FILTER_HIGH_PASS,
    DSP_FILTER_BAND_PASS
} dsp_filter_type_t;

typedef struct {
    dsp_filter_type_t type;
    float freq;     // Hz (20 - 20000)
    float gain_db;  // dB (-18 to +18)
    float q;        // Q factor (0.1 to 10.0)
    bool enabled;
} dsp_eq_band_t;

typedef struct {
    bool enabled;
    dsp_eq_band_t bands[DSP_EQ_NUM_BANDS];
} dsp_eq_config_t;

typedef struct {
    bool enabled;
    float threshold_db; // -60 dB to 0 dB
    float ratio;        // 1.0 to 20.0
    float attack_ms;    // 0.1 to 200 ms
    float release_ms;   // 10 to 2000 ms
    float knee_db;      // 0 dB (hard) to 20 dB (soft)
    float makeup_db;    // 0 dB to 24 dB
} dsp_compressor_config_t;

typedef struct {
    bool enabled;
    float ceiling_db;   // -6.0 dBFS to -0.1 dBFS
    float release_ms;   // 10 to 500 ms
} dsp_limiter_config_t;

typedef struct {
    bool enabled;
    float target_db;    // -24 to -12 dBFS
    float gate_db;      // -50 dBFS threshold below which audio is not boosted
    float speed;        // 0.1 (slow) to 1.0 (fast)
} dsp_loudness_config_t;

typedef struct {
    bool enabled;
    int8_t semitones;   // -12 to +12
    int8_t cents;       // -100 to +100
} dsp_pitch_config_t;

typedef struct {
    bool enabled;
    float freq;         // 4000 to 9000 Hz
    float threshold_db; // -40 to 0 dB
    float ratio;        // 2.0 to 10.0
} dsp_deesser_config_t;

typedef struct {
    bool enabled;
    float amount;       // 0.0 to 1.0
    float cutoff_hz;    // 600 to 1000 Hz (default ~700 Hz)
} dsp_crossfeed_config_t;

typedef struct {
    bool enabled;
    float width;        // 0.0 (mono) to 1.0 (normal) to 2.0 (super-wide)
} dsp_widener_config_t;

typedef struct {
    bool enabled;
    float drive;        // 0.0 to 1.0
    float warmth;       // 0.0 (3rd odd tape) to 1.0 (2nd even tube)
} dsp_tube_config_t;

typedef struct {
    bool enabled;
    float cutoff_hz;    // 60 to 140 Hz
    float drive;        // 0.0 to 1.0
    float blend;        // 0.0 to 1.0
} dsp_bass_config_t;

typedef struct {
    bool delay_enabled;
    float delay_time_ms;    // 10 to 1000 ms
    float delay_feedback;   // 0.0 to 0.85
    float delay_mix;        // 0.0 to 1.0
    bool reverb_enabled;
    float reverb_room_size; // 0.0 to 1.0
    float reverb_damping;   // 0.0 to 1.0
    float reverb_mix;       // 0.0 to 1.0
} dsp_reverb_delay_config_t;

typedef struct {
    float in_peak_l;
    float in_peak_r;
    float in_rms_l;
    float in_rms_r;
    float out_peak_l;
    float out_peak_r;
    float out_rms_l;
    float out_rms_r;
} dsp_meter_values_t;

typedef struct {
    bool bit_perfect_bypass;        // Master bit-perfect direct mode
    float master_volume_db;         // -60 dB to 0 dB
    dsp_eq_config_t eq;
    dsp_compressor_config_t comp;
    dsp_limiter_config_t limiter;
    dsp_loudness_config_t loudness;
    dsp_pitch_config_t pitch;
    dsp_deesser_config_t deesser;
    dsp_crossfeed_config_t crossfeed;
    dsp_widener_config_t widener;
    dsp_tube_config_t tube;
    dsp_bass_config_t bass;
    dsp_reverb_delay_config_t fx;
} dsp_config_t;

#ifdef __cplusplus
}
#endif

#endif /* _DSP_TYPES_H_ */
