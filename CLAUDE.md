# ECU Dashboard Project

## Goal

Build an ESP32-S3 based dashboard for ECUMaster EMU Classic.

Hardware:
- CrowPanel Advance 3.5 HMI (ESP32-S3)
- LVGL
- PlatformIO
- MAX3232 RS232-TTL adapter
- EMU Classic ECU firmware 1.211

## Current State

Working:
- Communication from ecu to terminal
- PlatformIO environment

Protocol:
- Use GTO2013/EMUSerial as reference implementation.
- Do not invent protocol details.
- Communication is read-only.

## Architecture

LVGL UI
    |
EngineDataModel
    |
EcuDataProvider
    |
EMUSerial Adapter
    |
UART1

## Rules

- No ECU write functions.
- Keep UI separated from protocol layer.
- Prefer small focused commits.
- Avoid putting everything into main.cpp.
- ESP32-S3 target.
- PlatformIO project.

## Next Task

Integrate EMUSerial and build EngineDataModel abstraction.