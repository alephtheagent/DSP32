# ESP32-S3 SuperMini Hi-Fi USB DAC & DSP Engine

Production-grade USB Audio Class (UAC 2.0) digital-to-analog converter and real-time DSP processor for the **ESP32-S3 SuperMini** with **PCM5102A + MAX97220**.

---

## 1. Hardware & Pinout

| ESP32-S3 SuperMini | PCM5102A / Module | Function |
|---|---|---|
| **GPIO 12** | **BCK** | I2S Bit Clock |
| **GPIO 13** | **DIN** | I2S Serial Data (PCM5102A Data Input) |
| **GPIO 14** | **LCK** | I2S Word Select / Frame Sync (LRCK) |
| **GND** | **SCK** | Tied to GND for internal PLL mode (no MCLK pin required) |
| **GND** | **GND** | System Ground (Analog + Digital) |
| **3.3V / 5V** | **VCC** | Power Supply (Clean LDO rail recommended) |
| **GPIO 19** | **USB D-** | USB 2.0 Full-Speed Data Minus (Native USB-C) |
| **GPIO 20** | **USB D+** | USB 2.0 Full-Speed Data Plus (Native USB-C) |
| **GPIO 0** | **Boot Button** | Toggles Wi-Fi SoftAP ON/OFF |

*Note*: No hardware mute pin, soft-volume ramping, anti-pop relays, or noise gates are used (direct bit-clean output path).

---

## 2. Architecture & Core Pinning

```
       +-------------------------------------------------------------+
       |                 ESP32-S3 DUAL-CORE XTENSA                   |
       |                                                             |
       |  CORE 0 (System & Network)     CORE 1 (Real-Time Audio)     |
       |  -------------------------     ------------------------     |
       |  - TinyUSB Stack (UAC2 + CDC)  - High-Priority DSP Task     |
       |  - Wi-Fi SoftAP Radio          - Format Conversion          |
       |  - Captive Portal DNS Server   - 11-Stage DSP Processing    |
       |  - HTTP Web Server & REST API  - Direct Bit-Perfect Bypass  |
       |  - USB CDC Serial CLI          - I2S Standard DMA Engine    |
       |  - Boot Button Debounce Task                                |
       +-------------------------------------------------------------+
                                      |
                      Lock-Free FreeRTOS Ring Buffer
                                      |
                                      v
                               I2S DMA Engine
                                      |
                           GPIO 12 (BCK), 13 (DIN), 14 (LCK)
                                      |
                                      v
                           PCM5102A DAC + MAX97220 Amp
```

- **Core 0**: Wi-Fi, HTTP Server, Captive Portal DNS, TinyUSB task, USB CDC CLI.
- **Core 1**: Real-Time Audio Task (Priority 21). Completely decoupled from Wi-Fi or Web traffic for zero audio dropouts.
- **Zero RF Interference Mode**: Wi-Fi SoftAP defaults to **OFF**. Pressing GPIO 0 Boot button turns it on. When all devices disconnect, the SoftAP automatically shuts down after 30 seconds to eliminate all RF noise from the MAX97220 / PCM5102A analog audio path!

---

## 3. Audio & USB Specifications

- **USB Audio Class**: UAC 2.0 Asynchronous Isochronous Audio with hardware feedback endpoint.
- **Sample Rates**: Dynamic support for **44.1 kHz**, **48.0 kHz**, and **96.0 kHz**.
- **Bit Depths**: **16-bit** and **24-bit** (in 32-bit slot).
- **Master Bit-Perfect Mode**: Hardware toggle at the top of Web UI / CLI. When active, DSP is 100% bypassed and incoming audio bytes stream directly to I2S DMA.
- **Dynamic Buffer Adjustment**:
  - **16 samples** (~0.33 ms, ultra-low latency mode: ~2.0 ms total roundtrip)
  - **32 samples** (~0.67 ms, ultra-low latency mode: ~3.0 ms total roundtrip)
  - **64 samples** (~1.33 ms, low latency mode)
  - **128 samples** (~2.67 ms, standard safe studio buffer)
  - **256 samples** (~5.33 ms, maximum jitter tolerance)
- **VU Level Metering**: Real-time stereo Peak & RMS meters (dBFS) for both Input and Output with ballistics decay.

---

## 4. DSP Effects Suite

1. **10-Band Parametric EQ**: RBJ biquad filters (Low Shelf, 8x Peaking, High Shelf) with smooth coefficient interpolation across audio frames (zero zipper noise).
2. **Studio Dynamic Compressor**: Soft/hard knee, adjustable threshold (-60 to 0 dB), ratio (1:1 to 20:1), attack (0.1 to 200 ms), release (10 to 2000 ms), makeup gain.
3. **Lookahead Brickwall Peak Limiter**: 64-sample lookahead window detects transients in advance, guaranteeing 0 dBFS ceiling protection and zero digital clipping.
4. **Loudness / AGC Normalizer**: Musical RMS volume leveler with noise gate (-50 dBFS) so background silence is never boosted.
5. **Real-time Pitch Shifter**: Dual-delay line with crossfade window for pitch shifting (+/- 12 semitones, fine cents).
6. **De-Esser**: Sidechain bandpass filter (4 kHz - 9 kHz) that dynamically compresses harsh sibilant vocal spikes.
7. **Headphone Crossfeed**: Bauer/Meier binaural head model blending delayed (~280us) low-pass filtered opposite channel to eliminate headphone listening fatigue.
8. **Mid-Side Stereo Widener**: Controls spatial soundstage width (0% mono to 200% ultra-wide).
9. **Cassette Tape Emulator**: Physically modeled vintage magnetic tape simulation featuring asymmetric magnetic hysteresis saturation, lo-fi frequency degradation (head gap loss HF roll-off 4-20 kHz and 65 Hz head bump), IEC shaped tape hiss, random soft clicks/pops, and mechanical wow & flutter warble.
10. **Psychoacoustic Bass Enhancer**: Generates upper harmonics of sub-bass frequencies (<100 Hz), allowing small headphones or drivers to perceive deep sub-bass via the "missing fundamental" effect.
11. **Ping-Pong Delay & Freeverb**:
    - Ping-pong stereo alternating delay line (up to 1000ms in PSRAM).
    - 8-comb / 4-allpass algorithmic Freeverb in PSRAM.
12. **Preset Management**: Factory presets (Reference Flat, Vintage Cassette, Vocal Presence, Bass Booster, Binaural Crossfeed, Night Normalizer) + user presets stored in NVS flash.

---

## 5. Web UI & Captive Portal

- Press **BOOT (GPIO 0)** button: Wi-Fi SoftAP starts (`SSID: DSP32-DAC`, no password).
- Captive Portal automatically redirects smartphones and laptops to `http://192.168.4.1/` (or browse directly to `http://dsp32.local/` via mDNS).
- Sleek, minimalist, dark monochrome design (`#0d0f12`).
- Interactive live EQ curve canvas, responsive sliders, real-time VU-meters, bidirectional state sync, and preset manager.
- When you disconnect your phone or laptop from the Wi-Fi network, the SoftAP automatically turns off after 30 seconds to ensure silent analog audio output without RF interference.

---

## 6. USB CDC Serial CLI

Connect to the USB CDC ACM COM port at 115200 baud:
- `help` - Show command manual
- `status` - Show sample rate, buffer size, latency, underruns, Wi-Fi status
- `bypass [on|off]` - Toggle Bit-Perfect bypass mode
- `buffer <16|32|64|128|256>` - Set buffer size in samples
- `vol <dB>` - Set master volume (-60.0 to 0.0 dB)
- `eq <band 0-9> <gain_db>` - Set EQ band gain
- `preset <list|load <idx>|save <idx> [name]|reset>` - Preset management
- `wifi <on|off|toggle|status>` - Control Wi-Fi SoftAP
- `vu` - Display live Peak & RMS values

---

## 7. Building & Flashing

```bash
# Load ESP-IDF environment
source /root/DSP32-S3/env.sh

# Target ESP32-S3
idf.py set-target esp32s3

# Build firmware
idf.py build

# Flash and monitor
idf.py -p /dev/ttyACM0 flash monitor
```

---

## 8. Continuous Integration & Releases (GitHub Actions)

The repository includes automated CI/CD via GitHub Actions (`.github/workflows/build.yml`):

- **Automated Builds**: Triggers on every `push` to `main`/`master`, all `pull_request` branches, and manual `workflow_dispatch`.
- **Pre-packaged Artifacts**:
  - `merged_firmware.bin` — Single unified binary containing bootloader, partition table, OTA data, and application. Flash in one command at offset `0x0`:
    ```bash
    esptool.py --chip esp32s3 -p /dev/ttyACM0 write_flash 0x0 merged_firmware.bin
    ```
  - `firmware_ota.bin` — Application image for OTA update via web UI (`/ota.html`).
- **Automated Releases**: Pushing a version tag (e.g. `git tag v0.1 && git push origin v0.1`) automatically creates a GitHub Release and attaches all binaries.

