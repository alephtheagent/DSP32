/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "audio_pipeline.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "i2s_dac.h"

static const char *TAG = "AUDIO_PIPE";

#define RINGBUF_SIZE_BYTES   (32 * 1024)
#define MAX_CHUNK_SAMPLES    256
#define DECLICK_RAMP_SAMPLES 64

// Inter-Core Ring Buffers:
// s_usb_rx_ringbuf: Filled by TinyUSB on Core 1, consumed by DSP Engine on Core 0
static RingbufHandle_t s_usb_rx_ringbuf = NULL;
// s_dsp_tx_ringbuf: Filled by DSP Engine on Core 0, consumed by I2S DMA on Core 1
static RingbufHandle_t s_dsp_tx_ringbuf = NULL;

static TaskHandle_t s_dsp_task_handle = NULL;
static TaskHandle_t s_i2s_task_handle = NULL;
static dsp_engine_t s_dsp_engine;

static volatile uint32_t s_sample_rate = 48000;
static volatile uint8_t  s_bit_depth = 16;
static volatile audio_buffer_size_t s_chunk_samples = LATENCY_MODE_SAFE_128;
static volatile uint32_t s_underruns = 0;
static volatile uint32_t s_overruns = 0;
static volatile bool     s_is_buffering = true;

// DSP Working Buffers (Core 0)
static float s_proc_l[MAX_CHUNK_SAMPLES];
static float s_proc_r[MAX_CHUNK_SAMPLES];
static uint8_t s_dsp_in_buf[MAX_CHUNK_SAMPLES * 8];
static uint8_t s_dsp_out_buf[MAX_CHUNK_SAMPLES * 8];

// I2S Output Buffer (Core 1)
static uint8_t s_i2s_out_buf[MAX_CHUNK_SAMPLES * 8];

//--------------------------------------------------------------------+
// De-clicking and Anti-pop Helpers
//--------------------------------------------------------------------+

// Fast PRNG for TPDF Dithering on Float -> PCM conversion
static inline float fast_tpdf_dither(uint32_t *prng_state)
{
    *prng_state = *prng_state * 1664525u + 1013904223u;
    uint32_t r1 = *prng_state;
    *prng_state = *prng_state * 1664525u + 1013904223u;
    uint32_t r2 = *prng_state;
    // Triangular distribution between -1.0 and +1.0 LSB
    return ((float)(int16_t)(r1 >> 16) - (float)(int16_t)(r2 >> 16)) * (1.0f / 65536.0f);
}

// Smoothly ramps up audio volume from 0 to full over min(DECLICK_RAMP_SAMPLES, num_samples)
static void pcm_apply_fade_in(void *buf, size_t num_samples, uint8_t bit_depth)
{
    size_t ramp_len = (num_samples < DECLICK_RAMP_SAMPLES) ? num_samples : DECLICK_RAMP_SAMPLES;
    if (ramp_len == 0) return;

    if (bit_depth == 16) {
        int16_t *s = (int16_t *)buf;
        for (size_t i = 0; i < ramp_len; i++) {
            float gain = (float)i / (float)ramp_len;
            s[2 * i]     = (int16_t)lrintf((float)s[2 * i] * gain);
            s[2 * i + 1] = (int16_t)lrintf((float)s[2 * i + 1] * gain);
        }
    } else {
        int32_t *s = (int32_t *)buf;
        for (size_t i = 0; i < ramp_len; i++) {
            float gain = (float)i / (float)ramp_len;
            s[2 * i]     = (int32_t)lrintf((float)s[2 * i] * gain);
            s[2 * i + 1] = (int32_t)lrintf((float)s[2 * i + 1] * gain);
        }
    }
}

// Generates a soft ramp down from (start_l, start_r) to 0 over min(DECLICK_RAMP_SAMPLES, num_samples)
static void pcm_generate_fade_out(void *buf, size_t num_samples, uint8_t bit_depth, int32_t start_l, int32_t start_r)
{
    size_t ramp_len = (num_samples < DECLICK_RAMP_SAMPLES) ? num_samples : DECLICK_RAMP_SAMPLES;

    if (bit_depth == 16) {
        int16_t *s = (int16_t *)buf;
        for (size_t i = 0; i < ramp_len; i++) {
            float gain = 1.0f - ((float)(i + 1) / (float)ramp_len);
            s[2 * i]     = (int16_t)lrintf((float)start_l * gain);
            s[2 * i + 1] = (int16_t)lrintf((float)start_r * gain);
        }
        for (size_t i = ramp_len; i < num_samples; i++) {
            s[2 * i]     = 0;
            s[2 * i + 1] = 0;
        }
    } else {
        int32_t *s = (int32_t *)buf;
        for (size_t i = 0; i < ramp_len; i++) {
            float gain = 1.0f - ((float)(i + 1) / (float)ramp_len);
            s[2 * i]     = (int32_t)lrintf((float)start_l * gain);
            s[2 * i + 1] = (int32_t)lrintf((float)start_r * gain);
        }
        for (size_t i = ramp_len; i < num_samples; i++) {
            s[2 * i]     = 0;
            s[2 * i + 1] = 0;
        }
    }
}

// Ramps down remainder of a partial buffer to 0 instead of abrupt step transition
static void pcm_fade_out_tail(void *buf, size_t bytes_collected, size_t bytes_needed, uint8_t bit_depth)
{
    size_t bytes_per_sample = (bit_depth == 24) ? 4 : 2;
    size_t bytes_per_frame = bytes_per_sample * 2;
    size_t valid_samples = bytes_collected / bytes_per_frame;
    size_t total_samples = bytes_needed / bytes_per_frame;

    if (valid_samples == 0) {
        memset(buf, 0, bytes_needed);
        return;
    }

    size_t remaining = (total_samples > valid_samples) ? (total_samples - valid_samples) : 0;
    if (remaining == 0) return;
    size_t ramp_len = (remaining < DECLICK_RAMP_SAMPLES) ? remaining : DECLICK_RAMP_SAMPLES;

    if (bit_depth == 16) {
        int16_t *s = (int16_t *)buf;
        int32_t start_l = s[2 * (valid_samples - 1)];
        int32_t start_r = s[2 * (valid_samples - 1) + 1];

        for (size_t i = 0; i < ramp_len; i++) {
            float gain = 1.0f - ((float)(i + 1) / (float)ramp_len);
            s[2 * (valid_samples + i)]     = (int16_t)lrintf((float)start_l * gain);
            s[2 * (valid_samples + i) + 1] = (int16_t)lrintf((float)start_r * gain);
        }
        for (size_t i = valid_samples + ramp_len; i < total_samples; i++) {
            s[2 * i]     = 0;
            s[2 * i + 1] = 0;
        }
    } else {
        int32_t *s = (int32_t *)buf;
        int32_t start_l = s[2 * (valid_samples - 1)];
        int32_t start_r = s[2 * (valid_samples - 1) + 1];

        for (size_t i = 0; i < ramp_len; i++) {
            float gain = 1.0f - ((float)(i + 1) / (float)ramp_len);
            s[2 * (valid_samples + i)]     = (int32_t)lrintf((float)start_l * gain);
            s[2 * (valid_samples + i) + 1] = (int32_t)lrintf((float)start_r * gain);
        }
        for (size_t i = valid_samples + ramp_len; i < total_samples; i++) {
            s[2 * i]     = 0;
            s[2 * i + 1] = 0;
        }
    }
}

// Records the last sample frame of a buffer
static inline void pcm_get_last_sample(const void *buf, size_t num_samples, uint8_t bit_depth, int32_t *out_l, int32_t *out_r)
{
    if (num_samples == 0) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    if (bit_depth == 16) {
        const int16_t *s = (const int16_t *)buf;
        *out_l = s[2 * (num_samples - 1)];
        *out_r = s[2 * (num_samples - 1) + 1];
    } else {
        const int32_t *s = (const int32_t *)buf;
        *out_l = s[2 * (num_samples - 1)];
        *out_r = s[2 * (num_samples - 1) + 1];
    }
}

//--------------------------------------------------------------------+
// CORE 0: DSP Worker Task (Priority 15)
// Consumes raw PCM from USB, executes float DSP chain, pushes to I2S ringbuf
//--------------------------------------------------------------------+
static void dsp_worker_task(void *pvParameters)
{
    ESP_LOGI(TAG, "DSP Compute Task started on Core %d (Priority 15)", xPortGetCoreID());
    uint32_t dither_state = 0x12345678;

    while (1) {
        size_t current_chunk = s_chunk_samples;
        uint8_t current_depth = s_bit_depth;
        size_t bytes_per_sample = (current_depth == 24) ? 4 : 2;
        size_t bytes_needed = current_chunk * 2 * bytes_per_sample; // Stereo

        // Read raw USB audio chunk from s_usb_rx_ringbuf
        size_t bytes_collected = 0;
        while (bytes_collected < bytes_needed) {
            size_t item_size = 0;
            size_t to_read = bytes_needed - bytes_collected;
            uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(s_usb_rx_ringbuf, &item_size, pdMS_TO_TICKS(10), to_read);
            if (item && item_size > 0) {
                memcpy(s_dsp_in_buf + bytes_collected, item, item_size);
                bytes_collected += item_size;
                vRingbufferReturnItem(s_usb_rx_ringbuf, item);
            } else {
                break;
            }
        }

        if (bytes_collected == 0) {
            // No USB data waiting - yield to other Core 0 services (Wi-Fi, HTTP, LED)
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (bytes_collected < bytes_needed) {
            // Smoothly ramp down remainder instead of abrupt DC zero-padding step
            pcm_fade_out_tail(s_dsp_in_buf, bytes_collected, bytes_needed, current_depth);
        }

        // 1. Bit-Perfect Direct Mode: Bypass all DSP calculations!
        if (dsp_engine_is_bit_perfect(&s_dsp_engine)) {
            dsp_engine_meter_update_pcm(&s_dsp_engine, s_dsp_in_buf, bytes_needed, current_depth);
            xRingbufferSend(s_dsp_tx_ringbuf, s_dsp_in_buf, bytes_needed, pdMS_TO_TICKS(20));
            continue;
        }

        // 2. DSP Mode: Convert PCM to Float
        if (current_depth == 16) {
            const int16_t *src = (const int16_t *)s_dsp_in_buf;
            const float norm16 = 1.0f / 32768.0f;
            for (size_t i = 0; i < current_chunk; i++) {
                s_proc_l[i] = (float)src[2 * i] * norm16;
                s_proc_r[i] = (float)src[2 * i + 1] * norm16;
            }
        } else {
            const int32_t *src = (const int32_t *)s_dsp_in_buf;
            const float norm24 = 1.0f / 8388608.0f;
            for (size_t i = 0; i < current_chunk; i++) {
                s_proc_l[i] = (float)(src[2 * i] >> 8) * norm24;
                s_proc_r[i] = (float)(src[2 * i + 1] >> 8) * norm24;
            }
        }

        // 3. Process complete studio 32-bit float DSP chain
        dsp_engine_process(&s_dsp_engine, s_proc_l, s_proc_r, current_chunk);

        // 4. Convert Float back to PCM with TPDF dither and clean silence floor
        if (current_depth == 16) {
            int16_t *dst = (int16_t *)s_dsp_out_buf;
            for (size_t i = 0; i < current_chunk; i++) {
                float fl = s_proc_l[i];
                float fr = s_proc_r[i];

                if (fl > 1.0f) fl = 1.0f;
                else if (fl < -1.0f) fl = -1.0f;
                if (fr > 1.0f) fr = 1.0f;
                else if (fr < -1.0f) fr = -1.0f;

                // Clean floor for absolute digital silence (below -120 dBFS)
                if (fabsf(fl) < 1e-6f) {
                    dst[2 * i] = 0;
                } else {
                    float d = fast_tpdf_dither(&dither_state);
                    float v = fl * 32767.0f + d;
                    if (v > 32767.0f) v = 32767.0f;
                    else if (v < -32768.0f) v = -32768.0f;
                    dst[2 * i] = (int16_t)lrintf(v);
                }

                if (fabsf(fr) < 1e-6f) {
                    dst[2 * i + 1] = 0;
                } else {
                    float d = fast_tpdf_dither(&dither_state);
                    float v = fr * 32767.0f + d;
                    if (v > 32767.0f) v = 32767.0f;
                    else if (v < -32768.0f) v = -32768.0f;
                    dst[2 * i + 1] = (int16_t)lrintf(v);
                }
            }
        } else {
            int32_t *dst = (int32_t *)s_dsp_out_buf;
            for (size_t i = 0; i < current_chunk; i++) {
                float fl = s_proc_l[i];
                float fr = s_proc_r[i];

                if (fl > 1.0f) fl = 1.0f;
                else if (fl < -1.0f) fl = -1.0f;
                if (fr > 1.0f) fr = 1.0f;
                else if (fr < -1.0f) fr = -1.0f;

                if (fabsf(fl) < 1e-7f) {
                    dst[2 * i] = 0;
                } else {
                    float v = fl * 8388607.0f;
                    if (v > 8388607.0f) v = 8388607.0f;
                    else if (v < -8388608.0f) v = -8388608.0f;
                    dst[2 * i] = ((int32_t)lrintf(v)) << 8;
                }

                if (fabsf(fr) < 1e-7f) {
                    dst[2 * i + 1] = 0;
                } else {
                    float v = fr * 8388607.0f;
                    if (v > 8388607.0f) v = 8388607.0f;
                    else if (v < -8388608.0f) v = -8388608.0f;
                    dst[2 * i + 1] = ((int32_t)lrintf(v)) << 8;
                }
            }
        }

        // Push processed audio to Core 1 I2S ringbuffer
        xRingbufferSend(s_dsp_tx_ringbuf, s_dsp_out_buf, bytes_needed, pdMS_TO_TICKS(20));
    }
}

//--------------------------------------------------------------------+
// CORE 1: Dedicated I2S Output Task (Priority 24 - REALTIME MAX)
// Feeds PCM5102A I2S DMA continuously with zero jitter and zero pops
//--------------------------------------------------------------------+
static void i2s_tx_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Dedicated I2S Output Task started on Core %d (Priority %d)",
             xPortGetCoreID(), configMAX_PRIORITIES - 1);

    bool stream_active = false;
    int32_t last_sample_l = 0;
    int32_t last_sample_r = 0;

    while (1) {
        size_t current_chunk = s_chunk_samples;
        uint8_t current_depth = s_bit_depth;
        size_t bytes_per_sample = (current_depth == 24) ? 4 : 2;
        size_t bytes_needed = current_chunk * 2 * bytes_per_sample;

        // 1. Watermark Pre-buffering when resuming from idle/stopped state
        if (s_is_buffering) {
            size_t free_sz = xRingbufferGetCurFreeSize(s_dsp_tx_ringbuf);
            size_t buffered = (free_sz < RINGBUF_SIZE_BYTES) ? (RINGBUF_SIZE_BYTES - free_sz) : 0;
            // 20ms jitter watermark cushion
            size_t watermark = (size_t)(s_sample_rate * 0.020f) * 2 * bytes_per_sample;
            if (watermark < bytes_needed * 4) watermark = bytes_needed * 4;
            if (watermark > RINGBUF_SIZE_BYTES / 2) watermark = RINGBUF_SIZE_BYTES / 2;

            if (buffered < watermark) {
                // Continuous silence to PCM5102A during prebuffering to maintain bit clock & PLL lock
                // NOTE: DO NOT call vTaskDelay here! i2s_dac_write blocks to pace DMA continuously.
                memset(s_i2s_out_buf, 0, bytes_needed);
                size_t written = 0;
                esp_err_t err = i2s_dac_write(s_i2s_out_buf, bytes_needed, &written, 100);
                if (err != ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                continue;
            }
            s_is_buffering = false;
        }

        // 2. Accumulate processed audio data from s_dsp_tx_ringbuf
        size_t bytes_collected = 0;
        while (bytes_collected < bytes_needed) {
            size_t item_size = 0;
            size_t to_read = bytes_needed - bytes_collected;
            // Short timeout (5ms) so we don't starve I2S DMA if audio stops
            uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(s_dsp_tx_ringbuf, &item_size, pdMS_TO_TICKS(5), to_read);
            if (item && item_size > 0) {
                memcpy(s_i2s_out_buf + bytes_collected, item, item_size);
                bytes_collected += item_size;
                vRingbufferReturnItem(s_dsp_tx_ringbuf, item);
            } else {
                break;
            }
        }

        // 3. Audio buffer ran dry / stopped
        if (bytes_collected == 0) {
            s_is_buffering = true;
            if (stream_active) {
                // Stream just stopped / paused: smoothly fade out to zero over 64 samples
                pcm_generate_fade_out(s_i2s_out_buf, current_chunk, current_depth, last_sample_l, last_sample_r);
                last_sample_l = 0;
                last_sample_r = 0;
                stream_active = false;
            } else {
                // Already in silence: output pure continuous zeroes
                memset(s_i2s_out_buf, 0, bytes_needed);
            }
            size_t written = 0;
            esp_err_t err = i2s_dac_write(s_i2s_out_buf, bytes_needed, &written, 100);
            if (err != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            // DO NOT call vTaskDelay here! i2s_dac_write keeps DMA continuously fed.
            continue;
        }

        // 4. Underrun: partial chunk received
        if (bytes_collected < bytes_needed) {
            s_underruns++;
            // Smoothly fade out remaining samples of this chunk to zero
            pcm_fade_out_tail(s_i2s_out_buf, bytes_collected, bytes_needed, current_depth);
            // End of active stream -> transition to buffering state for next stream
            last_sample_l = 0;
            last_sample_r = 0;
            stream_active = false;
            s_is_buffering = true;

            size_t written = 0;
            esp_err_t err = i2s_dac_write(s_i2s_out_buf, bytes_needed, &written, 100);
            if (err != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            continue;
        }

        // 5. Full valid chunk: if transitioning from silence to audio, apply soft fade-in
        if (!stream_active) {
            pcm_apply_fade_in(s_i2s_out_buf, current_chunk, current_depth);
            stream_active = true;
        }

        // Record the last sample of this chunk for smooth fade-out if stream stops next
        pcm_get_last_sample(s_i2s_out_buf, current_chunk, current_depth, &last_sample_l, &last_sample_r);

        // Output to PCM5102A I2S DMA (Core 1, hardware DMA)
        size_t written = 0;
        esp_err_t err = i2s_dac_write(s_i2s_out_buf, bytes_needed, &written, 100);
        if (err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

esp_err_t audio_pipeline_init(uint32_t sample_rate, uint8_t bit_depth)
{
    s_sample_rate = sample_rate;
    s_bit_depth = bit_depth;

    // 1. Create USB RX ringbuffer (receives from Core 1 TinyUSB)
    s_usb_rx_ringbuf = xRingbufferCreate(RINGBUF_SIZE_BYTES, RINGBUF_TYPE_BYTEBUF);
    if (!s_usb_rx_ringbuf) {
        ESP_LOGE(TAG, "Failed to create USB RX ringbuffer");
        return ESP_ERR_NO_MEM;
    }

    // 2. Create DSP TX ringbuffer (feeds Core 1 I2S DMA)
    s_dsp_tx_ringbuf = xRingbufferCreate(RINGBUF_SIZE_BYTES, RINGBUF_TYPE_BYTEBUF);
    if (!s_dsp_tx_ringbuf) {
        ESP_LOGE(TAG, "Failed to create DSP TX ringbuffer");
        return ESP_ERR_NO_MEM;
    }

    // 3. Initialize I2S hardware
    esp_err_t ret = i2s_dac_init(sample_rate, bit_depth);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S DAC: %s", esp_err_to_name(ret));
        return ret;
    }

    // 4. Initialize DSP Engine
    dsp_engine_init(&s_dsp_engine, (float)sample_rate);

    // 5. Spawn Core 0 DSP Worker Task (Priority 15)
    BaseType_t r = xTaskCreatePinnedToCore(dsp_worker_task,
                                           "dsp_worker",
                                           8192,
                                           NULL,
                                           15,
                                           &s_dsp_task_handle,
                                           0); // Core 0
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Core 0 DSP worker task");
        return ESP_FAIL;
    }

    // 6. Spawn Core 1 Dedicated I2S TX Task (Priority 24 - REALTIME MAX)
    r = xTaskCreatePinnedToCore(i2s_tx_task,
                                "i2s_tx",
                                4096,
                                NULL,
                                configMAX_PRIORITIES - 1,
                                &s_i2s_task_handle,
                                1); // Core 1
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Core 1 I2S TX task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Dual-Core Audio Engine initialized successfully:");
    ESP_LOGI(TAG, " -> Core 1: Dedicated Real-Time I/O (USB RX + I2S TX @ Priority 24)");
    ESP_LOGI(TAG, " -> Core 0: 32-bit DSP Engine + Network/Web UI (Priority 15)");
    return ESP_OK;
}

void audio_pipeline_write_usb_data(const uint8_t *data, size_t len, uint8_t bit_depth)
{
    if (!s_usb_rx_ringbuf || !data || len == 0) return;
    s_bit_depth = bit_depth;

    BaseType_t res = xRingbufferSend(s_usb_rx_ringbuf, data, len, 0);
    if (res != pdTRUE) {
        s_overruns++;
    }
}

void audio_pipeline_set_format(uint32_t sample_rate, uint8_t bit_depth)
{
    s_sample_rate = sample_rate;
    s_bit_depth = bit_depth;
    s_is_buffering = true;
    i2s_dac_set_clock(sample_rate, bit_depth);
    dsp_engine_set_sample_rate(&s_dsp_engine, (float)sample_rate);
}

void audio_pipeline_set_buffer_size(audio_buffer_size_t size)
{
    if (size != 16 && size != 32 && size != 64 && size != 128 && size != 256) {
        size = LATENCY_MODE_SAFE_128;
    }
    s_chunk_samples = size;
    ESP_LOGI(TAG, "Audio buffer latency changed: %d samples", (int)size);
}

audio_buffer_size_t audio_pipeline_get_buffer_size(void)
{
    return s_chunk_samples;
}

void audio_pipeline_get_stats(audio_pipeline_stats_t *out_stats)
{
    if (!out_stats) return;
    out_stats->buffer_size_samples = s_chunk_samples;
    out_stats->sample_rate = s_sample_rate;
    out_stats->bit_depth = s_bit_depth;
    out_stats->bit_perfect = dsp_engine_is_bit_perfect(&s_dsp_engine);
    out_stats->ringbuf_total_bytes = RINGBUF_SIZE_BYTES;
    out_stats->ringbuf_fill_bytes = s_dsp_tx_ringbuf ? (RINGBUF_SIZE_BYTES - xRingbufferGetCurFreeSize(s_dsp_tx_ringbuf)) : 0;
    out_stats->underrun_count = s_underruns;
    out_stats->overrun_count = s_overruns;
}

dsp_engine_t *audio_pipeline_get_dsp_engine(void)
{
    return &s_dsp_engine;
}
