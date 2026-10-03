/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "usb_descriptors.h"
#include "tusb.h"
#include "class/audio/audio.h"
#include <string.h>

// Device Descriptor
static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200, // USB 2.0 Full-Speed
    .bDeviceClass       = 0x00,   // Per-interface
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A, // Espressif VID
    .idProduct          = 0x4001, // USB Audio DAC PID
    .bcdDevice          = 0x0100, // v1.0
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

// Configuration Descriptor (187 bytes: 121 Audio + 66 CDC ACM)
static const uint8_t desc_fs_configuration[TOTAL_CONFIG_DESC_LEN] = {
    // 1. Configuration Descriptor (9 bytes)
    9, TUSB_DESC_CONFIGURATION,
    U16_TO_U8S_LE(TOTAL_CONFIG_DESC_LEN),
    ITF_NUM_TOTAL,      // bNumInterfaces = 4 (Audio Control + Streaming + CDC Control + CDC Data)
    1,                  // bConfigurationValue = 1
    0,                  // iConfiguration = 0
    0xC0,               // bmAttributes: Self-powered
    50,                 // bMaxPower: 100 mA

    // 2. Interface Association Descriptor (IAD) (8 bytes)
    8, TUSB_DESC_INTERFACE_ASSOCIATION,
    0,                  // bFirstInterface = 0
    2,                  // bInterfaceCount = 2
    TUSB_CLASS_AUDIO,   // bFunctionClass = 1
    AUDIO_SUBCLASS_CONTROL, // bFunctionSubClass = 1
    AUDIO_FUNC_PROTOCOL_CODE_UNDEF, // bFunctionProtocol = 0
    0,                  // iFunction = 0

    // 3. Interface 0, Alt 0: Audio Control (9 bytes)
    9, TUSB_DESC_INTERFACE,
    0,                  // bInterfaceNumber = 0
    0,                  // bAlternateSetting = 0
    0,                  // bNumEndpoints = 0
    TUSB_CLASS_AUDIO,   // bInterfaceClass = 1
    AUDIO_SUBCLASS_CONTROL, // bInterfaceSubClass = 1
    AUDIO_FUNC_PROTOCOL_CODE_UNDEF, // bInterfaceProtocol = 0
    4,                  // iInterface = 4 ("DSP32 Audio Control")

    // 4. CS AC Header Descriptor (9 bytes)
    9, TUSB_DESC_CS_INTERFACE,
    0x01,               // bDescriptorSubtype = HEADER (0x01)
    U16_TO_U8S_LE(0x0100), // bcdADC = 1.00 (UAC 1.0)
    U16_TO_U8S_LE(40),  // wTotalLength = 9 + 12 + 10 + 9 = 40 bytes
    1,                  // bInCollection = 1 streaming interface
    1,                  // baInterfaceNr[0] = 1

    // 5. Input Terminal Descriptor (USB Streaming) (12 bytes)
    12, TUSB_DESC_CS_INTERFACE,
    0x02,               // bDescriptorSubtype = INPUT_TERMINAL (0x02)
    UAC1_ENTITY_SPK_INPUT_TERMINAL, // bTerminalID = 1
    U16_TO_U8S_LE(AUDIO_TERM_TYPE_USB_STREAMING), // 0x0101
    0x00,               // bAssocTerminal = 0
    2,                  // bNrChannels = 2
    U16_TO_U8S_LE(0x0003), // wChannelConfig = Stereo (Left, Right)
    0x00,               // iChannelNames = 0
    0x00,               // iTerminal = 0

    // 6. Feature Unit Descriptor (Mute Only - Host controls volume digitally) (10 bytes)
    10, TUSB_DESC_CS_INTERFACE,
    0x06,               // bDescriptorSubtype = FEATURE_UNIT (0x06)
    UAC1_ENTITY_SPK_FEATURE_UNIT, // bUnitID = 2
    UAC1_ENTITY_SPK_INPUT_TERMINAL, // bSourceID = 1 (Input Terminal)
    1,                  // bControlSize = 1 byte
    0x01,               // bmaControls[0] (Master): bit 0 Mute only
    0x00,               // bmaControls[1] (Left)
    0x00,               // bmaControls[2] (Right)
    0x00,               // iFeature = 0

    // 7. Output Terminal Descriptor (Headphones) (9 bytes)
    9, TUSB_DESC_CS_INTERFACE,
    0x03,               // bDescriptorSubtype = OUTPUT_TERMINAL (0x03)
    UAC1_ENTITY_SPK_OUTPUT_TERMINAL, // bTerminalID = 3
    U16_TO_U8S_LE(AUDIO_TERM_TYPE_OUT_HEADPHONES), // 0x0302
    0x00,               // bAssocTerminal = 0
    UAC1_ENTITY_SPK_FEATURE_UNIT, // bSourceID = 2 (Feature Unit)
    0x00,               // iTerminal = 0

    // 8. Interface 1, Alt 0: Audio Streaming (Zero Bandwidth) (9 bytes)
    9, TUSB_DESC_INTERFACE,
    1,                  // bInterfaceNumber = 1
    0,                  // bAlternateSetting = 0
    0,                  // bNumEndpoints = 0
    TUSB_CLASS_AUDIO,   // bInterfaceClass = 1
    AUDIO_SUBCLASS_STREAMING, // bInterfaceSubClass = 2
    AUDIO_FUNC_PROTOCOL_CODE_UNDEF, // bInterfaceProtocol = 0
    0,                  // iInterface = 0

    // 9. Interface 1, Alt 1: Audio Streaming (Active 16-bit PCM Stereo) (9 bytes)
    9, TUSB_DESC_INTERFACE,
    1,                  // bInterfaceNumber = 1
    1,                  // bAlternateSetting = 1
    1,                  // bNumEndpoints = 1
    TUSB_CLASS_AUDIO,   // bInterfaceClass = 1
    AUDIO_SUBCLASS_STREAMING, // bInterfaceSubClass = 2
    AUDIO_FUNC_PROTOCOL_CODE_UNDEF, // bInterfaceProtocol = 0
    5,                  // iInterface = 5 ("16-bit 48kHz Stereo")

    // 10. CS AS General Descriptor (7 bytes)
    7, TUSB_DESC_CS_INTERFACE,
    0x01,               // bDescriptorSubtype = AS_GENERAL (0x01)
    UAC1_ENTITY_SPK_INPUT_TERMINAL, // bTerminalLink = 1 (Input Terminal 1)
    1,                  // bDelay = 1 ms
    U16_TO_U8S_LE(0x0001), // wFormatTag = PCM (0x0001)

    // 11. Type I Format Descriptor (14 bytes for 44.1k & 48k)
    14, TUSB_DESC_CS_INTERFACE,
    0x02,               // bDescriptorSubtype = FORMAT_TYPE (0x02)
    0x01,               // bFormatType = FORMAT_TYPE_I (0x01)
    2,                  // bNrChannels = 2
    2,                  // bSubFrameSize = 2 bytes
    16,                 // bBitResolution = 16 bits
    2,                  // bSamFreqType = 2 discrete frequencies
    0x44, 0xAC, 0x00,   // 44100 Hz (little-endian 24-bit)
    0x80, 0xBB, 0x00,   // 48000 Hz (little-endian 24-bit)

    // 12. Standard Audio Isochronous OUT Endpoint (9 bytes in UAC 1.0)
    9, TUSB_DESC_ENDPOINT,
    EPNUM_AUDIO_OUT,    // bEndpointAddress = EP 1 OUT
    0x09,               // bmAttributes: Isochronous (0x01) | Adaptive (0x08)
    U16_TO_U8S_LE(196), // wMaxPacketSize = 196 bytes
    1,                  // bInterval = 1 ms
    0,                  // bRefresh = 0
    0,                  // bSynchAddress = 0

    // 13. Class-Specific Audio Isochronous Endpoint (7 bytes)
    7, 0x25,            // bLength = 7, bDescriptorType = CS_ENDPOINT (0x25)
    0x01,               // bDescriptorSubtype = EP_GENERAL (0x01)
    0x01,               // bmAttributes = bit 0: Sampling Frequency Control
    0,                  // bLockDelayUnits = 0
    U16_TO_U8S_LE(0x0000), // wLockDelay = 0

    // 14. CDC ACM Interface Association & Control/Data Descriptors (66 bytes)
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_CONTROL, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_fs_configuration;
}

// String Descriptors
static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },    // 0: English (0x0409)
    "Espressif Systems",             // 1: Manufacturer
    "ESP32-S3 Hi-Fi DAC",            // 2: Product
    "DSP32-S3-001",                  // 3: Serial
    "DSP32 Audio Control",           // 4: Audio Control Interface
    "16-bit 48kHz Stereo",           // 5: Audio Streaming Interface
    "DSP32 Serial Control",          // 6: CDC Serial CLI Interface
};

static uint16_t _desc_str[64];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }

        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 63) chr_count = 63;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}

void usb_descriptors_init(void)
{
}
