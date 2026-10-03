# DSP32 
an USB DAC firmware for ESP32-S3 (specifically designed for Supermini) with fun effects. heavily vibecoded.
## features
- SSD1306 128x64 OLED support with u8g2
- utilizing onboard s3-supermini neopixel led (gpio 48) with visualizers
- web ui configuration hotspot on gpio 0 button (configurable)
- OTA updates (broken :P)
- LittleFS for presets
- fully configurable hardware gpios from the web ui (DAC, webui activation button, OLED)
## hardware
- (optional) SSD1306 128x64 OLED for status
- PCM5102A
- ESP32-S3 (made for supermini but should be compatible with others)
- (optional recommended for headphones) MAX97220 amp board. if you plan to use headphones you should have an amp board for it because the dac only outputs line out level and cannot drive headphones
# dsp effects
## actually useful
- 10 band eq: just a default eq. nothing to yap about.
- dynamic compressor: fixes quiet dialogue and loud gunshots so you stop fiddling with volume
- peak limiter: prevents your audio from clipping into ear-rape digital screech
- auto loudness: auto-levels tracks so quiet 70s songs don't get obliterated by loud modern tracks
## fun and games
- tape simulator: a tape simulator that can make your songs sound like they were found on a cassette during vietnam war
- bitcrusher: crushes your audio :D
- pitch shifter: makes your song sound more or less kawaii anime japan type shit
- reverb/delay: throws your sound into a giant cathedral or adds spacey echo trails
## psychoacoustic wizardry
- virtual bass: tricks your brain into thinking your small drivers have a subwoofer
- crossfeed: stops headphones from sounding like the band is playing inside your brain
- stereo widener: pushes stereo way past your ears for that IMAX theater feel

# DISCLAIMER
guys I don't really know if everything works as intended so if you encounter some issues or non working stuff let us know by writing an issue so we fix it :D

