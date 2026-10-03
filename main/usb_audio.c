/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "usb_audio.h"
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tusb.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "class/audio/audio.h"
#include "usb_descriptors.h"
#include "audio_pipeline.h"

static const char *TAG = "UAC1";

#define UAC1_MAX_PACKET_SZ  196

static usb_audio_status_t s_status = {
    .sample_rate = 48000,
    .bit_depth = 16,
    .bytes_per_sample = 2,
    .channels = 2,
    .is_streaming = false,
    .is_muted = false,
    .volume_db = 0,
    .frames_received = 0
};

static sample_rate_changed_cb_t s_sr_callback = NULL;

// DMA-aligned audio receive buffer for 1ms USB frames
static uint8_t s_rx_buf[UAC1_MAX_PACKET_SZ] __attribute__((aligned(4)));

// Control request temp buffer
static uint8_t s_ctrl_buf[64] __attribute__((aligned(4)));

// UAC 1.0 Endpoint Descriptor for usbd_edpt_open
static const tusb_desc_endpoint_t s_uac1_ep_desc = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = EPNUM_AUDIO_OUT,
    .bmAttributes     = { .xfer = TUSB_XFER_ISOCHRONOUS, .sync = 2 /* Adaptive */, .usage = 0 /* Data */ },
    .wMaxPacketSize   = tu_htole16(UAC1_MAX_PACKET_SZ),
    .bInterval        = 1
};

esp_err_t usb_audio_init(sample_rate_changed_cb_t sr_cb)
{
    s_sr_callback = sr_cb;
    ESP_LOGI(TAG, "USB Audio Class 1.0 Driver Initialized (44.1k/48k 16-bit Stereo Adaptive)");
    return ESP_OK;
}

void usb_audio_get_status(usb_audio_status_t *out_status)
{
    if (out_status) {
        *out_status = s_status;
    }
}

//--------------------------------------------------------------------+
// TinyUSB Custom Class Driver Implementation for UAC 1.0
//--------------------------------------------------------------------+

static void uac1_driver_init(void)
{
    s_status.is_streaming = false;
    s_status.frames_received = 0;
}

static void uac1_driver_reset(uint8_t rhport)
{
    (void)rhport;
    s_status.is_streaming = false;
}

static uint16_t uac1_driver_open(uint8_t rhport, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
    (void)rhport;

    // Verify this is the Audio Control interface (Class 1, SubClass 1, Protocol 0)
    if (desc_itf->bInterfaceClass != TUSB_CLASS_AUDIO ||
        desc_itf->bInterfaceSubClass != AUDIO_SUBCLASS_CONTROL ||
        desc_itf->bInterfaceProtocol != AUDIO_FUNC_PROTOCOL_CODE_UNDEF) {
        return 0;
    }

    // Total length covering AC Interface (49 bytes) + AS Alt0 (9 bytes) + AS Alt1 (46 bytes) = 104 bytes
    uint16_t drv_len = 104;
    TU_VERIFY(drv_len <= max_len, 0);

    ESP_LOGI(TAG, "UAC1 Driver Opened (length: %u bytes)", drv_len);
    return drv_len;
}

static bool uac1_driver_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    // Stage 1: SETUP
    if (stage == CONTROL_STAGE_SETUP) {
        // 1. Standard Interface Requests
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
            if (request->bRequest == TUSB_REQ_SET_INTERFACE) {
                uint8_t const itf = tu_u16_low(request->wIndex);
                uint8_t const alt = tu_u16_low(request->wValue);

                if (itf == ITF_NUM_AUDIO_STREAMING) {
                    if (alt == 1) {
                        // Open Isochronous OUT endpoint
                        usbd_edpt_open(rhport, &s_uac1_ep_desc);
                        s_status.is_streaming = true;
                        // Prime first 1ms packet receive
                        usbd_edpt_xfer(rhport, EPNUM_AUDIO_OUT, s_rx_buf, sizeof(s_rx_buf));
                        ESP_LOGI(TAG, "Audio Streaming STARTED (16-bit PCM @ %" PRIu32 " Hz)", s_status.sample_rate);
                    } else {
                        usbd_edpt_close(rhport, EPNUM_AUDIO_OUT);
                        s_status.is_streaming = false;
                        ESP_LOGI(TAG, "Audio Streaming STOPPED (Alt 0)");
                    }
                }
                tud_control_status(rhport, request);
                return true;
            } else if (request->bRequest == TUSB_REQ_GET_INTERFACE) {
                uint8_t const itf = tu_u16_low(request->wIndex);
                uint8_t cur_alt = (itf == ITF_NUM_AUDIO_STREAMING && s_status.is_streaming) ? 1 : 0;
                return tud_control_xfer(rhport, request, &cur_alt, 1);
            }
        }

        // 2. Class-Specific Requests
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
            uint8_t const req = request->bRequest;

            // Recipient: Interface (Audio Control - Feature Unit)
            if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE) {
                uint8_t const entity = tu_u16_high(request->wIndex);
                uint8_t const ctrl_sel = tu_u16_high(request->wValue);

                if (entity == UAC1_ENTITY_SPK_FEATURE_UNIT) {
                    // Mute Control (0x01)
                    if (ctrl_sel == 0x01) {
                        if (req == 0x01) { // SET_CUR
                            return tud_control_xfer(rhport, request, s_ctrl_buf, request->wLength);
                        } else if (req == 0x81) { // GET_CUR
                            uint8_t mute = s_status.is_muted ? 1 : 0;
                            return tud_control_xfer(rhport, request, &mute, 1);
                        }
                    }
                    // Volume Control (0x02)
                    else if (ctrl_sel == 0x02) {
                        if (req == 0x01) { // SET_CUR
                            return tud_control_xfer(rhport, request, s_ctrl_buf, request->wLength);
                        } else if (req == 0x81) { // GET_CUR
                            int16_t vol = tu_htole16(s_status.volume_db);
                            return tud_control_xfer(rhport, request, &vol, 2);
                        } else if (req == 0x82) { // GET_MIN (-50 dB = -12800)
                            int16_t min_vol = tu_htole16(-12800);
                            return tud_control_xfer(rhport, request, &min_vol, 2);
                        } else if (req == 0x83) { // GET_MAX (0 dB = 0)
                            int16_t max_vol = tu_htole16(0);
                            return tud_control_xfer(rhport, request, &max_vol, 2);
                        } else if (req == 0x84) { // GET_RES (1 dB = 256)
                            int16_t res_vol = tu_htole16(256);
                            return tud_control_xfer(rhport, request, &res_vol, 2);
                        }
                    }
                }
            }

            // Recipient: Endpoint (Sampling Frequency Control)
            if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT) {
                uint8_t const ctrl_sel = tu_u16_high(request->wValue);

                if (ctrl_sel == 0x01) { // SAMPLING_FREQ_CONTROL
                    if (req == 0x01) { // SET_CUR
                        return tud_control_xfer(rhport, request, s_ctrl_buf, request->wLength);
                    } else if (req == 0x81) { // GET_CUR
                        uint8_t sr[3] = {
                            (uint8_t)(s_status.sample_rate & 0xFF),
                            (uint8_t)((s_status.sample_rate >> 8) & 0xFF),
                            (uint8_t)((s_status.sample_rate >> 16) & 0xFF)
                        };
                        return tud_control_xfer(rhport, request, sr, 3);
                    }
                }
            }
        }
        return false; // Unsupported request -> STALL
    }

    // Stage 2: DATA or ACK (Host wrote data to device)
    if (stage == CONTROL_STAGE_ACK || stage == CONTROL_STAGE_DATA) {
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
            // Feature Unit data received
            if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE) {
                uint8_t const entity = tu_u16_high(request->wIndex);
                uint8_t const ctrl_sel = tu_u16_high(request->wValue);

                if (entity == UAC1_ENTITY_SPK_FEATURE_UNIT) {
                    if (ctrl_sel == 0x01) { // Mute
                        s_status.is_muted = (s_ctrl_buf[0] != 0);
                        ESP_LOGI(TAG, "Host Mute: %s", s_status.is_muted ? "MUTED" : "UNMUTED");
                    } else if (ctrl_sel == 0x02) { // Volume
                        s_status.volume_db = (int16_t)(s_ctrl_buf[0] | (s_ctrl_buf[1] << 8));
                    }
                    return true;
                }
            }

            // Endpoint data received (Sampling Frequency)
            if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT) {
                uint8_t const ctrl_sel = tu_u16_high(request->wValue);

                if (ctrl_sel == 0x01) { // SAMPLING_FREQ_CONTROL
                    uint32_t target_rate = (uint32_t)s_ctrl_buf[0] | ((uint32_t)s_ctrl_buf[1] << 8) | ((uint32_t)s_ctrl_buf[2] << 16);
                    if (target_rate == 44100 || target_rate == 48000) {
                        if (s_status.sample_rate != target_rate) {
                            ESP_LOGI(TAG, "Sample rate changed: %" PRIu32 " -> %" PRIu32 " Hz", s_status.sample_rate, target_rate);
                            s_status.sample_rate = target_rate;
                            if (s_sr_callback) {
                                s_sr_callback(s_status.sample_rate, s_status.bit_depth);
                            }
                        }
                    }
                    return true;
                }
            }
        }
    }

    return true;
}

static bool uac1_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)rhport;
    if (ep_addr == EPNUM_AUDIO_OUT) {
        if (result == XFER_RESULT_SUCCESS && xferred_bytes > 0) {
            s_status.frames_received++;
            if (s_status.is_muted) {
                memset(s_rx_buf, 0, xferred_bytes);
            }
            // Write received audio frame into pipeline ring buffer
            audio_pipeline_write_usb_data(s_rx_buf, xferred_bytes, 16);
        }

        // Keep receiving audio as long as host has streaming interface open
        if (s_status.is_streaming) {
            usbd_edpt_xfer(rhport, EPNUM_AUDIO_OUT, s_rx_buf, sizeof(s_rx_buf));
        }
        return true;
    }
    return false;
}

static const usbd_class_driver_t s_uac1_driver = {
    .name            = "UAC1",
    .init            = uac1_driver_init,
    .deinit          = NULL,
    .reset           = uac1_driver_reset,
    .open            = uac1_driver_open,
    .control_xfer_cb = uac1_driver_control_xfer_cb,
    .xfer_cb         = uac1_driver_xfer_cb,
    .xfer_isr        = NULL,
    .sof             = NULL
};

// Application driver callback replacing TinyUSB's weak stub
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &s_uac1_driver;
}
