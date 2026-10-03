/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "i2s_dac.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "storage/hw_config.h"

static const char *TAG = "I2S_DAC";
static i2s_chan_handle_t s_tx_chan = NULL;
static uint32_t s_current_rate = 48000;
static uint8_t s_current_depth = 16;
static uint32_t s_dma_desc_num = 8;
static uint32_t s_dma_frame_num = 240;

esp_err_t i2s_dac_init(uint32_t sample_rate, uint8_t bit_depth)
{
    s_current_rate = sample_rate;
    s_current_depth = bit_depth;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = s_dma_desc_num;
    chan_cfg.dma_frame_num = s_dma_frame_num;
    chan_cfg.auto_clear = true;

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_data_bit_width_t bit_width = (bit_depth == 24) ? I2S_DATA_BIT_WIDTH_24BIT : I2S_DATA_BIT_WIDTH_16BIT;
    // PCM5102A internal PLL (SCK tied to GND) strictly requires BCK >= 64*fs (32-bit slot width)
    i2s_slot_bit_width_t slot_width = I2S_SLOT_BIT_WIDTH_32BIT;

    const hw_config_t *hw = hw_config_get();
    int bck = (hw && hw->i2s_bck_gpio >= 0) ? hw->i2s_bck_gpio : I2S_DAC_BCK_IO;
    int din = (hw && hw->i2s_din_gpio >= 0) ? hw->i2s_din_gpio : I2S_DAC_DIN_IO;
    int ws  = (hw && hw->i2s_ws_gpio >= 0)  ? hw->i2s_ws_gpio  : I2S_DAC_WS_IO;

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bit_width, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)bck,
            .ws = (gpio_num_t)ws,
            .dout = (gpio_num_t)din,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.slot_cfg.slot_bit_width = slot_width;
    std_cfg.slot_cfg.ws_width = slot_width;

    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S initialized on BCK: GPIO%d, DIN: GPIO%d, WS: GPIO%d @ %" PRIu32 " Hz, %d-bit (64fs)",
             bck, din, ws, sample_rate, bit_depth);
    return ESP_OK;
}

esp_err_t i2s_dac_set_clock(uint32_t sample_rate, uint8_t bit_depth)
{
    if (!s_tx_chan) return ESP_ERR_INVALID_STATE;

    if (s_current_rate == sample_rate && s_current_depth == bit_depth) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Reconfiguring I2S clock: %" PRIu32 "Hz -> %" PRIu32 "Hz, %d-bit -> %d-bit (64fs)",
             s_current_rate, sample_rate, s_current_depth, bit_depth);

    i2s_channel_disable(s_tx_chan);

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    i2s_channel_reconfig_std_clock(s_tx_chan, &clk_cfg);

    i2s_data_bit_width_t bit_width = (bit_depth == 24) ? I2S_DATA_BIT_WIDTH_24BIT : I2S_DATA_BIT_WIDTH_16BIT;
    i2s_slot_bit_width_t slot_width = I2S_SLOT_BIT_WIDTH_32BIT;
    i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bit_width, I2S_SLOT_MODE_STEREO);
    slot_cfg.slot_bit_width = slot_width;
    slot_cfg.ws_width = slot_width;
    i2s_channel_reconfig_std_slot(s_tx_chan, &slot_cfg);

    i2s_channel_enable(s_tx_chan);

    s_current_rate = sample_rate;
    s_current_depth = bit_depth;
    return ESP_OK;
}

esp_err_t i2s_dac_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms)
{
    if (!s_tx_chan) return ESP_ERR_INVALID_STATE;
    return i2s_channel_write(s_tx_chan, src, size, bytes_written, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t i2s_dac_reconfig_buffer(uint32_t dma_desc_num, uint32_t dma_frame_num)
{
    if (s_dma_desc_num == dma_desc_num && s_dma_frame_num == dma_frame_num) {
        return ESP_OK;
    }

    s_dma_desc_num = dma_desc_num;
    s_dma_frame_num = dma_frame_num;

    if (s_tx_chan) {
        i2s_channel_disable(s_tx_chan);
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return i2s_dac_init(s_current_rate, s_current_depth);
    }
    return ESP_OK;
}
