/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _USB_AUDIO_H_
#define _USB_AUDIO_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate;       // Current sample rate (e.g. 44100, 48000, 96000)
    uint8_t  bit_depth;         // Current bit resolution (16 or 24)
    uint8_t  bytes_per_sample;  // 2 for 16-bit, 4 for 24-bit in 32-bit slot
    uint8_t  channels;          // 2 for Stereo
    bool     is_streaming;      // True if host is currently transmitting audio
    bool     is_muted;          // Mute flag from host
    int16_t  volume_db;         // Host volume in dB (8.8 fixed point or direct dB)
    uint32_t frames_received;   // Total audio frames received
} usb_audio_status_t;

typedef void (*sample_rate_changed_cb_t)(uint32_t new_rate, uint8_t new_depth);

esp_err_t usb_audio_init(sample_rate_changed_cb_t sr_cb);
void usb_audio_get_status(usb_audio_status_t *out_status);

#ifdef __cplusplus
}
#endif

#endif /* _USB_AUDIO_H_ */
