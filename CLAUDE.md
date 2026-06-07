# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

PortableAnki is a portable, offline Anki review terminal. The system has three parts: ESP32-S3 firmware (the handheld device), a planned Python bridge program (runs on the PC), and Anki's AnkiConnect plugin. The device downloads due cards over Wi-Fi, lets the user review them offline with physical buttons (Again/Hard/Good/Easy), then uploads the grades back when reconnected.

## Current State

This is an early-stage project. Be aware before assuming functionality exists:
- **`ESP4Anki/src/main.cpp` is unmodified PlatformIO boilerplate** (the `myFunction` stub) — no real firmware has been written yet.
- **The Python bridge does not exist yet** — it is planned but not started.
- **`PortableAnki_V1_Plan.md` (Chinese) is the authoritative design document.** It is the source of truth for intended behavior, hardware, wiring/GPIO assignments, and the staged development roadmap. Consult it before implementing device or bridge features.

## Commands

PlatformIO firmware (run from the `ESP4Anki/` directory):

```bash
pio run                  # build firmware
pio run -t upload        # build and flash to the board
pio device monitor       # open serial monitor
pio run -t clean         # clean build artifacts
```

The single build environment is `4d_systems_esp32s3_gen4_r8n16` (ESP32-S3, Arduino framework). If more environments are added later, target one with `-e <env>`.

## Architecture

Three-tier data flow — the device never touches the Anki database directly; all Anki access goes through the Python bridge and AnkiConnect:

```
PC Anki + AnkiConnect  ──HTTP──  Python bridge  ──Wi-Fi HTTP──  ESP32 device  ──SPI──  e-ink display
```

- **ESP32 device**: downloads `today.json`, reviews offline, stores grades to `review_log.jsonl` on LittleFS, uploads back on sync. Planned firmware modules: display, button scanning, card state machine, local JSON storage, Wi-Fi sync, review log.
- **Python bridge**: queries AnkiConnect for due cards, generates `today.json`, serves it over local HTTP, receives `review_log.jsonl`, and writes grades back into Anki via AnkiConnect.

V1 scope is text-only front/back, a single deck, offline review, and Wi-Fi sync. Explicitly out of scope for V1: audio, images, HTML/rich text, LaTeX, multi-deck filtering, on-device card editing, and direct AnkiWeb sync.

## Data Formats (device ↔ bridge contract)

`today.json` (downloaded by the device):

```json
{
  "deck": "English",
  "generatedAt": "2026-06-05T10:00:00+08:00",
  "cards": [
    { "cardId": 1498938915662, "front": "abandon", "back": "v. 放弃；遗弃" }
  ]
}
```

`review_log.jsonl` (uploaded by the device, one JSON object per line):

```json
{"cardId":1498938915662,"ease":3,"timeMs":8200}
```

`ease` is the grade: `1`=Again, `2`=Hard, `3`=Good, `4`=Easy. `timeMs` is the time from showing the front to grading.

## Planned Firmware Libraries

To be added to `lib_deps` in `ESP4Anki/platformio.ini` as firmware is built out: GxEPD2 (e-ink driver), ArduinoJson, ESP32 WiFi, HTTPClient, LittleFS.

See `PortableAnki_V1_Plan.md` for the hardware bill of materials, GPIO/wiring map, power design, enclosure, and the 9-stage build roadmap.
