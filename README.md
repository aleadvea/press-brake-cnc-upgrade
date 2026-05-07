# CNC Back Gauge Control System (Dual ESP32)

Production-oriented dual-controller firmware project for a CNC press brake back-gauge setup.

## Overview

This repository contains two coordinated firmware applications:

- `motor_brain/` — low-level motor controller firmware (ESP32)
- `motor_test/` — touchscreen HMI firmware (ESP32-S3 + LVGL)

The system is designed for reliable industrial motion control with clear HMI workflows, program/material management, and robust state handling in AUTO mode.

## Architecture

- Communication: ESP-NOW between HMI and motor controller
- HMI stack: LVGL 8 on Waveshare ESP32-S3 7" display
- Motor control: step/dir control with homing, limits, retract, alarm handling
- Persistence: LittleFS for settings, materials, programs, and selected language

## Key Capabilities

- Manual mode jogging with configurable increments
- Auto program execution by steps with bend/retract cycle logic
- Program and material CRUD workflows on touchscreen
- Homing flow, alarm flow, and emergency stop paths
- Language selection in Settings (Serbian / English) with persistent save

## Engineering Work Demonstrated

- Embedded state-machine design for motion workflows
- Real-time UI responsiveness optimization
- Communication protocol handling and synchronization
- Persistent configuration management on microcontroller
- Practical UX improvements for machine operators

## Repository Layout

- `motor_brain/`
  - `src/main.cpp`
  - `src/motor_ctrl.cpp`
  - `src/espnow_motor.cpp`
- `motor_test/`
  - `src/main.cpp`
  - `src/ui_*.cpp`
  - `src/storage.cpp`
  - `src/espnow_hmi.cpp`

## Build

Both firmware apps use PlatformIO.

- Motor controller project: `motor_brain/`
- HMI project: `motor_test/`

Typical commands:

```bash
pio run
pio run -t upload --upload-port COM3   # motor (example)
pio run -t upload --upload-port COM5   # HMI (example)
```

## Hardware Context

- Motor node: ESP32 + stepper driver + HOME/BEND/ALARM inputs
- HMI node: Waveshare ESP32-S3 Touch LCD 7

## Status

Active development with real hardware testing and iterative UI/firmware improvements.

## Author

Firmware/automation project built as a practical CNC-focused embedded portfolio project.
