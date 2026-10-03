/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dsp_stereo_widener.h"

void dsp_stereo_widener_init(dsp_stereo_widener_t *widener)
{
    widener->config.enabled = false;
    widener->config.width = 1.25f;
}

void dsp_stereo_widener_update_config(dsp_stereo_widener_t *widener, const dsp_widener_config_t *config)
{
    widener->config = *config;
}

void dsp_stereo_widener_process(dsp_stereo_widener_t *widener, float *buf_l, float *buf_r, size_t num_samples)
{
    if (!widener->config.enabled) return;

    float width = widener->config.width;
    if (width < 0.0f) width = 0.0f;
    if (width > 2.0f) width = 2.0f;

    for (size_t i = 0; i < num_samples; i++) {
        float l = buf_l[i];
        float r = buf_r[i];

        float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r);

        side *= width;

        buf_l[i] = mid + side;
        buf_r[i] = mid - side;
    }
}
