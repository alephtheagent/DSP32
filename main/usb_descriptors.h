/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _USB_DESCRIPTORS_H_
#define _USB_DESCRIPTORS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Interface numbers
enum {
    ITF_NUM_AUDIO_CONTROL = 0,
    ITF_NUM_AUDIO_STREAMING,
    ITF_NUM_CDC_CONTROL,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

// Audio Entities (UAC 1.0)
#define UAC1_ENTITY_SPK_INPUT_TERMINAL  0x01
#define UAC1_ENTITY_SPK_FEATURE_UNIT    0x02
#define UAC1_ENTITY_SPK_OUTPUT_TERMINAL 0x03

// Endpoints
#define EPNUM_AUDIO_OUT                 0x01
#define EPNUM_CDC_NOTIF                 0x82
#define EPNUM_CDC_OUT                   0x03
#define EPNUM_CDC_IN                    0x83

// Configuration Descriptor Length (Audio UAC 1.0 = 121 + CDC ACM = 66)
#define TOTAL_CONFIG_DESC_LEN           (121 + TUD_CDC_DESC_LEN)

void usb_descriptors_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _USB_DESCRIPTORS_H_ */
