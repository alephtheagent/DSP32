/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_engine.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "DSP_ENGINE";

void dsp_engine_init(dsp_engine_t *engine, float sample_rate)
{
    engine->sample_rate = sample_rate > 0 ? sample_rate : 48000.0f;
    engine->lock = xSemaphoreCreateMutex();

    memset(&engine->config, 0, sizeof(engine->config));
    engine->config.bit_perfect_bypass = false;
    engine->config.master_volume_db = 0.0f;
    engine->master_vol_linear = 1.0f;

    dsp_eq_init(&engine->eq, engine->sample_rate);
    engine->config.eq = engine->eq.config;

    dsp_compressor_init(&engine->comp, engine->sample_rate);
    engine->config.comp = engine->comp.config;

    dsp_limiter_init(&engine->limiter, engine->sample_rate);
    engine->config.limiter = engine->limiter.config;

    dsp_loudness_init(&engine->loudness, engine->sample_rate);
    engine->config.loudness = engine->loudness.config;

    dsp_pitch_init(&engine->pitch, engine->sample_rate);
    engine->config.pitch = engine->pitch.config;

    dsp_deesser_init(&engine->deesser, engine->sample_rate);
    engine->config.deesser = engine->deesser.config;

    dsp_crossfeed_init(&engine->crossfeed, engine->sample_rate);
    engine->config.crossfeed = engine->crossfeed.config;

    dsp_stereo_widener_init(&engine->widener);
    engine->config.widener = engine->widener.config;

    dsp_tube_warmth_init(&engine->tube);
    engine->config.tube = engine->tube.config;

    dsp_bass_enhancer_init(&engine->bass, engine->sample_rate);
    engine->config.bass = engine->bass.config;

    dsp_reverb_delay_init(&engine->fx, engine->sample_rate);
    engine->config.fx = engine->fx.config;

    dsp_meter_init(&engine->meter, engine->sample_rate);

    ESP_LOGI(TAG, "DSP Engine initialized @ %.1f kHz", engine->sample_rate / 1000.0f);
}

void dsp_engine_set_sample_rate(dsp_engine_t *engine, float sample_rate)
{
    if (xSemaphoreTake(engine->lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        engine->sample_rate = sample_rate;
        dsp_eq_set_sample_rate(&engine->eq, sample_rate);
        dsp_compressor_set_sample_rate(&engine->comp, sample_rate);
        dsp_limiter_set_sample_rate(&engine->limiter, sample_rate);
        dsp_loudness_set_sample_rate(&engine->loudness, sample_rate);
        dsp_pitch_set_sample_rate(&engine->pitch, sample_rate);
        dsp_deesser_set_sample_rate(&engine->deesser, sample_rate);
        dsp_crossfeed_set_sample_rate(&engine->crossfeed, sample_rate);
        dsp_bass_enhancer_set_sample_rate(&engine->bass, sample_rate);
        dsp_reverb_delay_set_sample_rate(&engine->fx, sample_rate);
        dsp_meter_set_sample_rate(&engine->meter, sample_rate);
        xSemaphoreGive(engine->lock);
        ESP_LOGI(TAG, "DSP sample rate updated to %.1f kHz", sample_rate / 1000.0f);
    }
}

void dsp_engine_set_config(dsp_engine_t *engine, const dsp_config_t *config)
{
    if (xSemaphoreTake(engine->lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        engine->config = *config;
        engine->master_vol_linear = powf(10.0f, config->master_volume_db / 20.0f);

        dsp_eq_update_config(&engine->eq, &config->eq);
        dsp_compressor_update_config(&engine->comp, &config->comp);
        dsp_limiter_update_config(&engine->limiter, &config->limiter);
        dsp_loudness_update_config(&engine->loudness, &config->loudness);
        dsp_pitch_update_config(&engine->pitch, &config->pitch);
        dsp_deesser_update_config(&engine->deesser, &config->deesser);
        dsp_crossfeed_update_config(&engine->crossfeed, &config->crossfeed);
        dsp_stereo_widener_update_config(&engine->widener, &config->widener);
        dsp_tube_warmth_update_config(&engine->tube, &config->tube);
        dsp_bass_enhancer_update_config(&engine->bass, &config->bass);
        dsp_reverb_delay_update_config(&engine->fx, &config->fx);

        xSemaphoreGive(engine->lock);
    }
}

void dsp_engine_get_config(dsp_engine_t *engine, dsp_config_t *out_config)
{
    if (!out_config) return;
    if (xSemaphoreTake(engine->lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        *out_config = engine->config;
        xSemaphoreGive(engine->lock);
    }
}

void dsp_engine_set_bit_perfect(dsp_engine_t *engine, bool bypass)
{
    if (xSemaphoreTake(engine->lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        engine->config.bit_perfect_bypass = bypass;
        xSemaphoreGive(engine->lock);
        ESP_LOGI(TAG, "Bit-Perfect Bypass: %s", bypass ? "ENABLED (DIRECT I2S)" : "DISABLED (DSP ACTIVE)");
    }
}

bool dsp_engine_is_bit_perfect(dsp_engine_t *engine)
{
    return engine->config.bit_perfect_bypass;
}

void dsp_engine_set_master_volume(dsp_engine_t *engine, float volume_db)
{
    if (volume_db > 0.0f) volume_db = 0.0f;
    if (volume_db < -60.0f) volume_db = -60.0f;

    if (xSemaphoreTake(engine->lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        engine->config.master_volume_db = volume_db;
        engine->master_vol_linear = powf(10.0f, volume_db / 20.0f);
        xSemaphoreGive(engine->lock);
    }
}

void dsp_engine_get_meters(dsp_engine_t *engine, dsp_meter_values_t *out_meters)
{
    dsp_meter_get_values(&engine->meter, out_meters);
}

void dsp_engine_meter_update_pcm(dsp_engine_t *engine, const uint8_t *pcm, size_t bytes, uint8_t bit_depth)
{
    dsp_meter_update_pcm(&engine->meter, pcm, bytes, bit_depth);
}

void dsp_engine_process(dsp_engine_t *engine, float *buf_l, float *buf_r, size_t num_samples)
{
    if (num_samples == 0) return;

    // 1. Input Metering (capture clean input)
    dsp_meter_update_input(&engine->meter, buf_l, buf_r, num_samples);

    // If Bit-Perfect Bypass is active, skip all DSP calculations!
    if (engine->config.bit_perfect_bypass) {
        dsp_meter_update_output(&engine->meter, buf_l, buf_r, num_samples);
        return;
    }

    // 2. 10-Band Parametric EQ
    dsp_eq_process(&engine->eq, buf_l, buf_r, num_samples);

    // 3. Dynamic Compressor
    dsp_compressor_process(&engine->comp, buf_l, buf_r, num_samples);

    // 4. Pitch Shifter
    dsp_pitch_process(&engine->pitch, buf_l, buf_r, num_samples);

    // 5. De-Esser
    dsp_deesser_process(&engine->deesser, buf_l, buf_r, num_samples);

    // 6. Psychoacoustic Bass Enhancer
    dsp_bass_enhancer_process(&engine->bass, buf_l, buf_r, num_samples);

    // 7. Tube Warmth / Tape Saturation
    dsp_tube_warmth_process(&engine->tube, buf_l, buf_r, num_samples);

    // 8. Mid-Side Stereo Widener
    dsp_stereo_widener_process(&engine->widener, buf_l, buf_r, num_samples);

    // 9. Headphone Crossfeed
    dsp_crossfeed_process(&engine->crossfeed, buf_l, buf_r, num_samples);

    // 10. Reverb & Delay
    dsp_reverb_delay_process(&engine->fx, buf_l, buf_r, num_samples);

    // 11. AGC / Loudness Normalizer
    dsp_loudness_process(&engine->loudness, buf_l, buf_r, num_samples);

    // 12. Lookahead Brickwall Peak Limiter
    dsp_limiter_process(&engine->limiter, buf_l, buf_r, num_samples);

    // 13. Master Volume Scaling
    float vol = engine->master_vol_linear;
    if (vol != 1.0f) {
        for (size_t i = 0; i < num_samples; i++) {
            buf_l[i] *= vol;
            buf_r[i] *= vol;
        }
    }

    // 14. Output Metering (capture final processed audio)
    dsp_meter_update_output(&engine->meter, buf_l, buf_r, num_samples);
}
