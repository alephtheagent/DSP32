/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#ifndef _I2S_DAC_H_
#define _I2S_DAC_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// PCM5102A Pinout on ESP32-S3 SuperMini (Shifted by -1)
#define I2S_DAC_BCK_IO      10  // Bit Clock
#define I2S_DAC_DIN_IO      11  // Data Out from S3 -> Data In on PCM5102A
#define I2S_DAC_WS_IO       12  // Word Select / LRCK

esp_err_t i2s_dac_init(uint32_t sample_rate, uint8_t bit_depth);
esp_err_t i2s_dac_set_clock(uint32_t sample_rate, uint8_t bit_depth);
esp_err_t i2s_dac_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms);
esp_err_t i2s_dac_reconfig_buffer(uint32_t dma_desc_num, uint32_t dma_frame_num);

#ifdef __cplusplus
}
#endif

#endif /* _I2S_DAC_H_ */
