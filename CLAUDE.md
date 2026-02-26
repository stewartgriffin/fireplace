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
| Flap Actuators (2x) | GPIO (2 pins each) | Actuators for fireplace and house air flaps |

### Communication Protocols
- **I2C**: HD44780 LCD display (via PCF8574 I2C expander), DS3231 RTC
- **SPI**: MAX6675 thermocouple interface
- **GPIO**: Flap actuator control (open pin + close pin per flap)

### Flap GPIO Pin Assignments
| Flap | Open Pin | Close Pin |
|------|----------|-----------|
| Ventilation flap | PB7 | PB6 |
| Fireplace flap | PC5 | PA10 |

## Development Environment

- **IDE/Tools**: STM32CubeMX + CMake
- **Build System**: CMake
- **Configuration**: STM32CubeMX (.ioc file)
- **Version Control**: Git

## Project Structure

```
Components/
├── ds3231/           # DS3231 RTC driver
├── gui/              # GUI system with focus and blinking fields
├── hd44780/          # HD44780 LCD display driver
├── max6675/          # MAX6675 thermocouple driver
├── pcf8574/          # PCF8574 I2C GPIO expander driver
├── ui/               # UI module with button handling and callbacks
├── flap_controller/  # Dual-GPIO flap controller (open/close pins, timed actuation)
├── daily_schedule/   # Time-based scheduling with duty cycle control
├── logic/            # Main application logic (fireplace.c)
Core/
├── Src/              # STM32 HAL and peripheral initialization
│   ├── i2c.c        # I2C configuration
│   └── spi.c        # SPI configuration
fireplace.ioc         # STM32CubeMX project configuration
```

## Current Status

**Development Stage**: Core features working

### Completed Features
- ✅ SPI communication with MAX6675 thermocouple (interrupt-driven)
- ✅ Basic peripheral initialization
- ✅ FreeRTOS integration
- ✅ I2C configuration and communication
- ✅ HD44780 LCD display integration (via PCF8574 I2C expander)
- ✅ DS3231 RTC integration for timekeeping
- ✅ GUI system with focus management and field blinking
- ✅ UI module with button handling (up, down, left, right)
- ✅ Callback-based event system for button presses
- ✅ Real-time temperature reading from MAX6675
- ✅ Dual-GPIO flap controller with timed open/close actuation
  - Separate configurable travel times for open and close directions
  - End-stop protection: targets 0% and 100% use `TRAVEL_TIME_MAX_MS` (5000ms) to absorb accumulated timing error
  - Mid-motion direction change: stops motor, recalculates position from elapsed time, starts new motion
  - Simultaneous-pin safety: always deactivates both pins before asserting either direction
- ✅ Daily schedule module with 30-minute duty cycle control
- ✅ Doxygen documentation for all driver modules

### In Development
- Fireplace temperature monitoring logic and control algorithms
- Integration of daily schedule with ventilation flap control (ventilation flap currently held fully open)
- Final integration of all components into cohesive control system

### Planned Features
- Air quality sensor integration (PM2.5, CO2, or VOC sensors)
- Intelligent scheduling based on air quality measurements
- Advanced control algorithms for flap management
- Enhanced user interface features (menus, settings)

## User Interface System

### UI Module (ui.c/ui.h)
- **Button Input Handling**: Supports 4 directional buttons (up, down, left, right)
- **Callback Architecture**: Event-driven design with registered callbacks
  - `short_press_up()`, `short_press_down()`, `short_press_left()`, `short_press_right()`
  - `long_press_left_and_right()` for special functions (e.g., entering time-set mode)
- **Debouncing**: 50ms debounce time to filter mechanical switch noise
- **Long Press Detection**: 1000ms threshold for long press recognition
- **Data Structure Pattern**: Follows ds3231 driver pattern with function pointers in `ui_data_t` struct

### GUI Module (gui.c/gui.h)
- **Display Management**: Manages screen buffer for HD44780 LCD
- **Focus System**: Supports field selection for editing time/date
  - Focus modes: `GUI_FOCUS_HOUR`, `GUI_FOCUS_MINUTE`, `GUI_FOCUS_SECOND`, `GUI_FOCUS_DAY`, `GUI_FOCUS_MONTH`, `GUI_FOCUS_YEAR`
- **Field Blinking**: Visual feedback for focused field
  - Configurable on/off timing (currently 800ms/800ms)
  - Backup/restore mechanism to handle continuous display updates
  - Works seamlessly with real-time data updates
- **Time Display**: Formats and displays current time from DS3231 RTC
- **Status Display**: Shows fireplace temperature, ventilation status, and sensor readings

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

### Recent Progress (2026-02-26)
- ✅ Flap actuators replaced: PWM servo → dual-GPIO (open pin + close pin)
- ✅ Flap controller rewritten with state machine (IDLE / OPENING / CLOSING)
- ✅ Separate open and close travel times supported per flap instance
- ✅ End-stop protection via `TRAVEL_TIME_MAX_MS` for 0% and 100% targets
- ✅ Ventilation flap GPIO pins commented out pending hardware verification
- ✅ Ventilation flap set to fully open (100%) on startup

### Previous Progress (2025-12-23)
- ✅ UI module completed with callback-based button handling
- ✅ GUI focus system implemented with field blinking for time editing
- ✅ MAX6675 interrupt-driven SPI communication fully operational
- ✅ HD44780 LCD display working via PCF8574 I2C GPIO expander
- ✅ DS3231 RTC integrated and providing accurate timekeeping
- ✅ All peripheral drivers following consistent data structure pattern

### Key Considerations
- Real-time operation with FreeRTOS task management
- Reliable temperature sensing for safety
- Timed GPIO control for flap actuation with end-stop error correction
- Scheduled operations using DS3231 RTC
- Future-proof design for air quality sensor integration

### Important Technical Notes

#### STM32CubeMX Interrupt Configuration
**Critical**: When using interrupt-driven peripherals (SPI, I2C, UART), ensure the interrupt is enabled in STM32CubeMX:
- Navigate to the peripheral configuration in CubeMX
- Check the "Interrupt" checkbox under NVIC settings
- Generate code to add proper IRQ handler to `stm32h5xx_it.c`
- Missing this step causes `transfer_in_progress` flags to remain stuck
- System may appear to work with debugger (timing changes) but fail in normal operation

#### Interrupt Priority
All peripheral interrupts use priority 5 to ensure FreeRTOS compatibility:
- SPI2 (MAX6675): Priority 5
- I2C2 (DS3231): Priority 5
- I2C3 (PCF8574/HD44780): Priority 5
- This matches `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`

#### GUI Blinking with Continuous Updates
The GUI blink system uses a backup/restore pattern to handle continuous data updates:
- Focused field content backed up when visible
- Update calls conditionally skip writing when field is hidden (blink off)
- Allows real-time data refresh without interfering with blink effect

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

**Last Updated**: 2026-02-26
