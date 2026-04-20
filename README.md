# Anemoia-ESP32

This is a fork of [https://github.com/Shim06/Anemoia-ESP32](https://github.com/Shim06/Anemoia-ESP32), an NES emulator for ESP32.

## Changes Made

- **PlatformIO Build**: Migrated the project to use PlatformIO via `platformio.ini`, so dependencies and board configuration are managed centrally. This makes it easier to change build flags, switch ESP32 variants, or add custom library options without the Arduino IDE.
- **Display Driver**: Replaced `TFT_eSPI` with the Adafruit ST77xx driver family. Display initialization and pixel drawing now use Adafruit-compatible routines, so any further display porting should focus on the display abstraction layer rather than the emulator core.
- **Screen Size**: Added support for 128x128 rendering in addition to the original display mode. The renderer now handles smaller resolution output and scales the graphics to match the display dimensions.
- **Controls**: Replaced the original button pad with a custom analog joystick design that uses a 6-direction joy pad and resistor network, plus two push buttons. The input code reads ADC voltages and button states, so modifying controller behavior typically means updating the analog threshold mapping and input-to-NES-button translation.
- **Storage**: Added support for loading ROMs from LittleFS as an alternative to SD card. The filesystem mounting and ROM loading paths are now configurable, making it easier to adapt the project to flash-based storage.

For more details on the original project, including hardware setup, performance, and compatibility, please refer to the [original repository](https://github.com/Shim06/Anemoia-ESP32).