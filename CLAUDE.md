# Fireplace & Ventilation Controller Project

## Project Overview

This is an intelligent controller for a fireplace and house ventilation system. The system monitors fireplace temperature and controls air flaps to optimize both fireplace efficiency and indoor air quality.

### Main Functionality

1. **Fireplace Air Control**
   - Monitors fireplace temperature using MAX6675 thermocouple sensor
   - Opens air flap when fire is burning to supply oxygen
   - Closes flap when fire is out to prevent excessive cooling from outside air
   - Prevents heat loss through the fireplace chimney

2. **House Ventilation Control**
   - Controls a second flap for fresh air intake to the house
   - Implements time-based scheduling to open flap during optimal air quality times
   - Future: Will integrate air quality sensor readings into decision logic
   - Balances scheduled ventilation with real-time air quality measurements

## Hardware Configuration

### Microcontroller
- **MCU**: STM32H5 series
- **RTOS**: FreeRTOS

### Peripherals

| Component | Interface | Purpose |
|-----------|-----------|---------|
| MAX6675 | SPI | Thermocouple temperature sensor for fireplace |
| HD44780 LCD | I2C (via PCF8574) | Display for status and temperature readings |
| DS3231 | I2C | Real-time clock for scheduling |
| Servo Motors (2x) | PWM | Actuators for fireplace and house air flaps |

### Communication Protocols
- **I2C**: HD44780 LCD display (via PCF8574 I2C expander), DS3231 RTC
- **SPI**: MAX6675 thermocouple interface
- **PWM**: Servo motor control for both flaps

## Development Environment

- **IDE/Tools**: STM32CubeMX + CMake
- **Build System**: CMake
- **Configuration**: STM32CubeMX (.ioc file)
- **Version Control**: Git

## Project Structure

```
Components/
├── hd44780/          # LCD display driver
├── logic/            # Main application logic (fireplace.c)
Core/
├── Src/              # STM32 HAL and peripheral initialization
│   └── i2c.c        # I2C configuration
fireplace.ioc         # STM32CubeMX project configuration
```

## Current Status

**Development Stage**: Core features working

### Completed Features
- ✅ SPI communication with MAX6675 thermocouple
- ✅ Basic peripheral initialization
- ✅ FreeRTOS integration
- ✅ I2C configuration

### In Development
- HD44780 LCD display integration
- Fireplace temperature monitoring logic
- Air flap control logic
- DS3231 RTC integration
- Scheduling system for ventilation

### Planned Features
- Air quality sensor integration
- Intelligent scheduling based on air quality measurements
- Advanced control algorithms
- User interface on LCD

## Technical Decisions & Preferences

### Coding Style
- Follow existing code patterns in the project
- Use STM32 HAL library for peripheral access
- Keep embedded C code straightforward and maintainable
- No over-engineering - focus on reliability

### Control Logic Philosophy
- **Fireplace Flap**: Temperature-based control
  - Open when fire detected (high temperature)
  - Close when fire is out
  - Prevent unnecessary heat loss

- **House Ventilation Flap**: Time + Quality based control
  - Scheduled opening during statistically optimal air quality times
  - Override based on air quality sensor readings (when implemented)
  - Safety and efficiency balanced approach

## Development Notes

### Recent Progress
- MAX6675 SPI communication successfully implemented
- Peripherals configured in STM32CubeMX
- FreeRTOS added to project

### Key Considerations
- Real-time operation with FreeRTOS task management
- Reliable temperature sensing for safety
- Servo position control for precise flap operation
- Scheduled operations using DS3231 RTC
- Future-proof design for air quality sensor integration

## Safety & Reliability Requirements

- Accurate temperature sensing for fireplace monitoring
- Fail-safe flap positions if system fails
- Reliable RTC for consistent scheduling
- Robust error handling for sensor failures

## Future Enhancements

1. Air quality sensor integration (PM2.5, CO2, or VOC)
2. Data logging capabilities
3. Remote monitoring/control (optional)
4. Advanced scheduling algorithms
5. Self-learning optimal ventilation times
6. Temperature history and analytics

---

**Last Updated**: 2025-12-22
