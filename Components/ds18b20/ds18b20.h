/***********************************************************************************************************************
 *
 *            File: ds18b20.h
 *      Created on: Mar 3, 2026
 *          Author: Claude Code
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

#ifndef COMPONENTS_DS18B20_DS18B20_H_
#define COMPONENTS_DS18B20_DS18B20_H_

/**************************************           INCLUDE FILES              ******************************************/
#include <stdint.h>
#include <stdbool.h>

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief Internal state of the DS18B20 driver
 *
 * The driver sequences through these states to perform a full temperature read:
 *   IDLE → RESET_CONVERT → CMD_CONVERT → CONVERTING → RESET_READ → CMD_READ → READING → IDLE
 */
typedef enum {
    DS18B20_STATE_IDLE,           // Waiting for the next read cycle
    DS18B20_STATE_RESET_CONVERT,  // Reset pulse sent, waiting for presence response
    DS18B20_STATE_CMD_CONVERT,    // Skip ROM + Convert T command sent, waiting for completion
    DS18B20_STATE_CONVERTING,     // Temperature conversion in progress (~750 ms)
    DS18B20_STATE_RESET_READ,     // Reset pulse sent before scratchpad read
    DS18B20_STATE_CMD_READ,       // Skip ROM + Read Scratchpad command sent
    DS18B20_STATE_READING,        // Reading temperature bytes from scratchpad
} ds18b20_state_t;

/**
 * @brief DS18B20 driver data structure
 *
 * Each instance is independent.  Initialise with ds18b20_init() before use.
 *
 * UART wiring (single-wire / half-duplex):
 *   - uart_transmit_receive() sends bytes on TX and simultaneously captures the
 *     line state on RX (echo + device pull-downs).  This matches the STM32 HAL
 *     HAL_UART_TransmitReceive_IT() pattern used elsewhere in the project.
 *   - uart_set_baudrate() is called by the driver to switch between the 1-Wire
 *     reset baud rate (9600) and the data baud rate (115200).
 */
typedef struct {
    // State machine
    ds18b20_state_t state;

    // Results
    int32_t temperature;      // Last measured temperature in whole °C
    bool sensor_present;      // True if sensor responded to the last reset pulse

    // Timing
    uint32_t conversion_start_tick;  // HAL tick when Convert T was issued
    uint32_t last_read_tick;         // HAL tick when the last read cycle completed

    // Buffers (largest transfer: 16 bytes for 2 encoded DS18B20 bytes)
    uint8_t tx_buffer[16];
    uint8_t rx_buffer[16];

    // Hardware abstraction
    int  (*uart_transmit_receive)(uint8_t *tx, uint8_t *rx, uint16_t size);
    void (*uart_set_baudrate)(uint32_t baudrate);
} ds18b20_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialise the DS18B20 driver
 * @param data               Pointer to the driver data structure
 * @param uart_transmit_receive  Interrupt-driven UART transmit-receive function.
 *                           Sends @p size bytes from @p tx and captures the bus
 *                           response in @p rx simultaneously.  Must call
 *                           ds18b20_interrupt() when the transfer completes.
 * @param uart_set_baudrate  Function to change the UART baud rate.  Called with
 *                           9600 before each reset pulse and 115200 before data.
 */
void ds18b20_init(ds18b20_data_t *data,
                  int  (*uart_transmit_receive)(uint8_t *tx, uint8_t *rx, uint16_t size),
                  void (*uart_set_baudrate)(uint32_t baudrate));

/**
 * @brief Main function — call from the application loop as fast as possible
 * Manages timing: starts a new read cycle every DS18B20_READ_PERIOD_MS and
 * advances the CONVERTING state once the conversion time has elapsed.
 * @param data Pointer to the driver data structure
 */
void ds18b20_main(ds18b20_data_t *data);

/**
 * @brief UART interrupt handler — call from the UART TX/RX complete IRQ callback
 * Advances the state machine after each UART transfer completes.
 * @param data Pointer to the driver data structure
 */
void ds18b20_interrupt(ds18b20_data_t *data);

/**
 * @brief Get the last measured temperature
 * @param data Pointer to the driver data structure
 * @return Temperature in whole °C (updated every DS18B20_READ_PERIOD_MS)
 */
int32_t ds18b20_get_temperature(ds18b20_data_t *data);

/**
 * @brief Check whether the sensor responded during the last reset pulse
 * @param data Pointer to the driver data structure
 * @return true if the sensor is present, false if no presence pulse was detected
 */
bool ds18b20_is_sensor_present(ds18b20_data_t *data);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_DS18B20_DS18B20_H_ */
