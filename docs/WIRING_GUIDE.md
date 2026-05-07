# Wiring Guide (CNC Press Brake Back-Gauge)

This guide documents the practical wiring used in this project so another person can reproduce the setup.

## 1. Safety First

- Disconnect mains power before rewiring.
- Verify voltage rails with a multimeter before connecting ESP boards.
- Test movement at low speed first.
- Keep E-STOP in hardware path (independent from firmware).

## 2. Power Topology

- Driver power: 36V PSU to motor driver AC terminals.
- Logic power: 24V PSU into LM2596 buck, adjusted to 5.0V output.
- Shared GND: PSU GND, buck GND, both ESP GNDs, sensor GND, driver ALM-.

## 3. Motor Driver Signal Philosophy

- `PUL+`, `DIR+`, `ENA+` receive +5V rail.
- ESP GPIO controls `PUL-`, `DIR-` (optocoupler low-side control).
- Hardware E-STOP is placed in `5V -> PUL+` path.

Result:

- E-STOP immediately blocks motion pulses in hardware.
- Motor remains position-locked by driver torque hold.

## 4. ESP32 Motor Node (`motor_brain`)

Main pins:

- `GPIO18` -> `PUL-`
- `GPIO19` -> `DIR-`
- `GPIO21` -> (not used for active ENA control in current safe setup)
- `GPIO22` -> HOME sensor input (`INPUT_PULLUP`, active LOW)

See project source comments and firmware logic in:

- `motor_brain/src/main.cpp`
- `motor_brain/src/motor_ctrl.cpp`

## 5. Encoder to Driver

Encoder wiring is connected directly to driver encoder terminals (`VCC`, `EGND`, `EA+/EA-`, `EB+/EB-`) according to motor/driver pinout.

Do not feed encoder from random external rail unless required by your exact driver model.

## 6. Inductive Sensors Through Optocoupler Board

Sensors are 24V side, ESP is 3.3V side.

Typical approach used here:

- Sensor side to optocoupler input (`INx`, `G`)
- ESP side reads optocoupler output (`Vx`) with pull-up logic
- Active detection interpreted as LOW in firmware

Recommended HMI/motor validation:

1. Check HOME sensor transitions.
2. Check BEND sensor transitions.
3. Confirm alarm input behavior.

## 7. Communication and Dual-Node Link

- HMI node and motor node communicate via ESP-NOW.
- HMI sends commands and settings sync packets.
- Motor node reports status packets (position, state, homed, alarm).

## 8. Commissioning Checklist

1. Flash `motor_brain` (COM3 by default).
2. Flash `motor_test` (COM5 by default).
3. Boot both boards.
4. Verify live position and state updates on HMI.
5. Run homing cycle.
6. Test manual jog.
7. Test one short AUTO program.

## 9. Notes

- Exact wire colors can differ by motor/encoder/sensor vendor.
- Always trust continuity tests and terminal labels over color assumptions.
- If direction is inverted, swap direction logic in firmware or motor phase orientation according to driver documentation.
