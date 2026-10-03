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
    ITF_NUM_TOTAL
};

// Audio Entities (UAC 1.0)
#define UAC1_ENTITY_SPK_INPUT_TERMINAL  0x01
#define UAC1_ENTITY_SPK_FEATURE_UNIT    0x02
#define UAC1_ENTITY_SPK_OUTPUT_TERMINAL 0x03

// Endpoints
#define EPNUM_AUDIO_OUT                 0x01

// Configuration Descriptor Length
// 9 (Cfg) + 8 (IAD) + 9 (AC) + 9 (CS AC Hdr) + 12 (In Term) + 10 (FU) + 9 (Out Term)
// + 9 (AS Alt0) + 9 (AS Alt1) + 7 (CS AS Gen) + 14 (Type I) + 9 (EP) + 7 (CS EP) = 121
#define UAC1_CONFIG_DESC_LEN            121

void usb_descriptors_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _USB_DESCRIPTORS_H_ */
