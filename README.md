# Abrams Pet v1.2 — side panel prototype

ESP32-S3 / Waveshare 2.13 V4 (250x122), GPIO mapping unchanged from uploaded v1.1.

## Screen
160x122 monochrome portrait on left, 90px status column on right. Portraits are center-cropped from previously approved preview sprites without further stretching. Panel labels use built-in ASCII font to avoid missing Cyrillic glyphs. UP is uptime, not wall-clock time. ENERGY and FOOD are simple simulated counters, not measured sensors. WiFi AP means the firmware runs a local access point, not internet status. No battery percent is shown.

## OTA / safety
Existing `/update` and Wi-Fi AP routes are preserved in source. This project has not been compiled or tested on hardware. Check the OTA partition layout of the installed firmware before updating. Keep v1.1 backup. Upload only `.pio/build/esp32s3/firmware.bin` to OTA; **never** upload merged `Abrams-Pet-full.bin` via OTA.

## GitHub Actions from iPhone
Upload repository files, including hidden `.github/workflows/build.yml`, to GitHub; run `Build Abrams Pet firmware` from Actions; download artifact, extract `firmware.bin`. Flash through Safari while connected to ESP32 access point, after verifying compatibility.
