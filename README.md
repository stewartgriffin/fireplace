# Fireplace & Ventilation Controller

An intelligent controller for a wood-burning fireplace and house ventilation system, built on STM32H5. The system monitors exhaust gas temperature, manages combustion air and fresh-air flaps, communicates with an external display over RS485, and provides a real-time local UI on an HD44780 LCD.

---

## Hardware

| Component | Interface | Role |
|-----------|-----------|------|
| STM32H533RE (Nucleo) | — | Main MCU, FreeRTOS |
| MAX6675 | SPI2 | Thermocouple — exhaust gas temperature |
| DS18B20 | UART6 (1-Wire) | Ambient external temperature |
| DS3231 | I2C2 | Real-time clock for scheduling |
| HD44780 (4×20) | I2C3 via PCF8574 | Local status display |
| Flap actuators ×2 | GPIO | Fireplace and ventilation air flaps |
| Waveshare 27479 | UART4 / RS485 | External display communication |

### Flap GPIO Assignments

| Flap | Open pin | Close pin |
|------|----------|-----------|
| Fireplace | PC5 | PA10 |
| Ventilation | PB7 | PB6 |

---

## LCD Display

The 4×20 HD44780 display shows real-time system status. All fields update continuously; time and date fields blink when in edit mode.

<p align="center">
  <img src="docs/lcd_screen.svg" alt="HD44780 4×20 LCD screen" width="485"/>
</p>

| Field | Label | Description |
|-------|-------|-------------|
| `K%` | Fireplace flap | Current fireplace flap position (0–100 %) |
| `Kom t` | Exhaust temperature | Thermocouple reading in °C (MAX6675) |
| `W%` | Ventilation flap | Current ventilation flap position (0–100 %) |
| `Zew t` | External temperature | Outside temperature in °C with one decimal (DS18B20) |
| `najw` | Session peak | Highest exhaust temperature recorded (stored in NVM) |
| Row 4 | Time / Date | `HH:MM:SS` and `DD.MM.YY` from DS3231 RTC |

---

## Combustion State Machine

The fireplace flap is driven exclusively by the combustion state — not raw temperature — to ensure safe, efficient airflow at every stage of a burn cycle.

```
                       temp > 37°C
    ┌─────────────────────────────────────────────┐
    │                                             ▼
  [OFF]   startup req.                        [STARTUP]
    ▲    ──────────────────────────────────►   flap 100%
    │                                             │
    │  temp < 30°C                     temp > 80°C│
    │  (grace expired)                            ▼
    │                                         [WORKING]
    │                                          flap 100%
    │                                             │
    │                              temp < 70°C   │
    │                                            ▼
    │         temp > 77°C               [ENDING]
    │    (no end req.) ◄──────────────  flap 30%
    │                                             │
    │                              temp < 50°C   │
    │                                            ▼
    └──────────────────────────────────      [COOL_DOWN]
           temp < 30°C                        flap 0%

  Any state: temp > 120°C  ──►  [PROTECTION]  flap 20–100%
             temp < 113°C  ──►  back to WORKING
```

**Key behaviours:**
- **Startup grace period** — 30 minutes after a startup request the flap stays open regardless of temperature, preventing premature closure on a cold fire.
- **End delay** — after an end request the controller waits 90 minutes before forcing the ENDING state, allowing a long burn to finish naturally.
- **Overtemperature protection** — above 120 °C the flap is throttled linearly from 100 % down to 20 % at 130 °C and the state machine is bypassed until the temperature drops below 113 °C.
- **Session dT/dt tracking** — exhaust temperature derivative is computed over 10 s, 20 s and 30 s windows at 1 Hz and exposed via the RS485 protocol for display-side trend analysis.

---

## Ventilation Logic

The ventilation flap follows a priority chain:

1. **Hot summer day** (`May 15 – Sep 15`, temp ≥ 14 °C) → 100 % open, otherwise 30 %.
2. **Warm daytime** (`08:00–21:00`): 50–100 % depending on external temperature.
3. **Fire active** (`STARTUP / WORKING / PROTECTION`) → duty-cycle schedule peaking at 100 %.
4. **Default** → time-of-day duty cycle schedule:

| Time | Duty | Max open |
|------|------|----------|
| 00:00 | 10 % | 40 % |
| 01:00 | 0 % | closed |
| 08:00 | 50 % | 50 % |
| 15:00 | 0 % | closed |
| 22:00 | 10 % | 100 % |

---

## RS485 Communication Protocol

The controller communicates with an external display over UART4 / RS485 at 115 200 baud (Waveshare 27479 auto-direction module on PC10/PC11).

### Frame format

```
[SOF = 0xAA] [MSG_ID] [LEN] [PAYLOAD × LEN] [CRC16_HIGH] [CRC16_LOW]
```

CRC16-Modbus covers `MSG_ID + LEN + PAYLOAD`.

### Message table

| ID | Direction | Name | Payload |
|----|-----------|------|---------|
| `0x01` | RX | `FIREPLACE_ENABLE` | — |
| `0x02` | RX | `FIREPLACE_DISABLE` | — |
| `0x03` | RX | `VENTILATION_ENABLE` | — |
| `0x04` | RX | `VENTILATION_DISABLE` | — |
| `0x05` | RX | `TIME_REQUEST` | — |
| `0x06` | RX | `STATUS_REQUEST` | — |
| `0x07` | RX | `DTDT_REQUEST` | — |
| `0x81` | TX | `ACK` | 1 B: echoed MSG_ID |
| `0x82` | TX | `NACK` | 1 B: echoed MSG_ID |
| `0x83` | TX | `TIME_RESPONSE` | 7 B: year (uint16 BE) · month · day · hour · min · sec |
| `0x84` | TX | `STATUS_RESPONSE` | 7 B: ext_temp (int16 BE, tenths °C) · exhaust_temp (int16 BE, tenths °C) · vent_pct · fire_pct · combustion_state |
| `0x85` | TX | `DTDT_RESPONSE` | 12 B: dTdt_10s · dTdt_20s · dTdt_30s · max_10s · max_20s · max_30s (all int16 BE, °C/min) |

All multi-byte integers are big-endian.

---

## Project Structure

```
Components/
├── combustion_controller/   # Fireplace state machine + dT/dt analysis
├── comm/                    # RS485 half-duplex protocol driver
├── daily_schedule/          # Duty-cycle ventilation scheduler
├── ds18b20/                 # 1-Wire temperature sensor (UART6 half-duplex)
├── ds3231/                  # RTC driver (I2C2)
├── flap_controller/         # Dual-GPIO timed flap actuator
├── gui/                     # LCD screen buffer and diff-based update engine
├── hd44780/                 # HD44780 character display driver
├── logic/                   # Main application (fireplace.c)
├── max6675/                 # SPI thermocouple driver
├── nvm_manager/             # Non-volatile storage (highest temperature)
├── pcf8574/                 # I2C GPIO expander driver
└── ui/                      # Button debounce, long-press, and callbacks
Core/
└── Src/                     # STM32 HAL and CubeMX-generated peripheral init
fireplace.ioc                # STM32CubeMX project configuration
```

---

## Build

The project uses STM32CubeMX for peripheral configuration and CMake for building.

```bash
# Generate HAL sources from CubeMX first, then:
cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
cmake --build build
```

Flash the resulting `.elf` with STM32CubeProgrammer or OpenOCD.

---

## License

All rights reserved. © Tomasz Ziajko
