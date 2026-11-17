# Fenik Pet v1.4.2 OTA Recovery — candidate build

Source based on user-provided v1.4.1 Sleep Fix archive. Keeps existing WebUI, art and gameplay. Fixes early actionFace declaration, aligns SLEEP/SLEEPY face IDs with their actual bitmap arrays, starts Wi-Fi before first e-ink render, adds /update OTA endpoint and GitHub Actions build.

**Important:** Not compiled or tested on hardware. `board_build.partitions = default.csv` is an assumption, NOT verified against the currently installed Abrams Pet OTA partition scheme. Confirm target ESP32-S3 flash size, partition table, and OTA slot before flashing. If partitions differ, adjust this project first. Save current Abrams firmware as recovery fallback. Existing WebUI has no visible OTA button: open http://192.168.4.1/update directly after installation.

Wi-Fi may still become unavailable if the synchronous e-ink driver blocks, if the board resets, or if there is a power issue. This patch does not guarantee a fix for overnight Wi-Fi disappearance. More robust display scheduling and recovery require hardware tests.

## WebUI v1.4.3 tabs
Mobile navigation: Главная, Status, Игры, Debug, Настройки. Existing endpoints and UI element IDs retained. OTA link at Settings -> OTA Update. Not hardware-tested or compiled.

## v1.4.4 — Character portrait in WebUI
The main page now displays Fenik's original embedded monochrome portrait instead of the text face. The portrait is served from flash at `/portrait?face=N` (no SPIFFS upload needed), and updates with `/status` every 2 seconds. Existing five embedded web PNG variants are reused for the 12 expressions (other expressions use nearest available portrait). During quiz, the portrait stays neutral to avoid revealing the answer. OTA and tabs remain unchanged.

**Important:** This firmware has not been compiled or hardware-tested. Verify partition compatibility with the Abrams Pet firmware before OTA.
