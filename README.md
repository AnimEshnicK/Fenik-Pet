# Abrams Pet v1.0 (experimental)

ESP32-S3 DevKitC-1 + Waveshare 2.13 V4 e-ink, 250×122.

Pin assumptions (verify against the real device): MOSI 11, SCK 12, CS 10, DC 9, RST 8, BUSY 7.

This prototype is based on the sketch that compiled in Wokwi. A successful cloud build does not verify the actual display, wiring, or Wi-Fi behavior.

## Build without local software

GitHub → Actions → Build Abrams Pet firmware → Run workflow → download artifact `Abrams-Pet-ESP32S3`.

For a full flash install with Espressif esptool-js, select `Abrams-Pet-full.bin` at address **0x0**. **Do not use the application-only firmware.bin at 0x0.**

Warning: full flash install overwrites the prior Fenik Pet bootloader/partition table/application; save a backup if you want to restore it. Flash only after verifying the target board and pinout.

The original Wokwi sketch does not include OTA and uses a basic face drawing. Wi-Fi AP is `Abrams-Pet`, password `AbramsPet2026` (as hardcoded in sketch).
