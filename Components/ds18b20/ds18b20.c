/***********************************************************************************************************************
 *
 *            File: ds18b20.c
 *      Created on: Mar 3, 2026
 *          Author: Claude Code
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "ds18b20.h"
#include "stm32h5xx_hal.h"

/**************************************           DEFINES                    ******************************************/

// 1-Wire over UART baud rates
#define DS18B20_BAUD_RESET   9600U    // Used for the reset/presence pulse
#define DS18B20_BAUD_DATA   115200U   // Used for all data bit transfers

// DS18B20 ROM and function commands
#define DS18B20_CMD_SKIP_ROM        0xCCU
#define DS18B20_CMD_CONVERT_T       0x44U
#define DS18B20_CMD_READ_SCRATCHPAD 0xBEU

// Timing
#define DS18B20_CONVERSION_TIME_MS  750U   // 12-bit resolution conversion time
#define DS18B20_READ_PERIOD_MS     2000U   // Interval between temperature reads

// Reset pulse byte and presence detection
// At 9600 baud: sending 0xF0 produces a reset waveform.
// If the received byte equals 0xF0 the bus was never pulled low → no device.
// Any other value means the sensor pulled the line low → presence detected.
#define DS18B20_RESET_BYTE       0xF0U
#define DS18B20_NO_PRESENCE_BYTE 0xF0U

// Buffer sizes
#define DS18B20_ENCODED_BYTE_SIZE  8U   // 1 data byte encodes to 8 UART bytes
#define DS18B20_CMD_BUFFER_SIZE   16U   // Skip ROM (8) + command (8)
#define DS18B20_TEMP_BUFFER_SIZE  16U   // 2 temperature bytes = 16 bits = 16 UART bytes

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static void encode_byte(uint8_t byte, uint8_t *buf);
static int32_t decode_temperature(const uint8_t *buf);
static void start_reset(ds18b20_data_t *data, ds18b20_state_t next_state);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/

void ds18b20_init(ds18b20_data_t *data,
                  int  (*uart_transmit_receive)(uint8_t *tx, uint8_t *rx, uint16_t size),
                  void (*uart_set_baudrate)(uint32_t baudrate))
{
    data->uart_transmit_receive = uart_transmit_receive;
    data->uart_set_baudrate = uart_set_baudrate;
    data->state = DS18B20_STATE_IDLE;
    data->temperature = 0;
    data->sensor_present = false;
    data->conversion_start_tick = 0;

    // Set last_read_tick so the first read starts immediately on the first main() call
    data->last_read_tick = HAL_GetTick() - DS18B20_READ_PERIOD_MS;
}

void ds18b20_main(ds18b20_data_t *data)
{
    if (data->state == DS18B20_STATE_IDLE)
    {
        uint32_t elapsed = HAL_GetTick() - data->last_read_tick;
        if (elapsed >= DS18B20_READ_PERIOD_MS)
        {
            start_reset(data, DS18B20_STATE_RESET_CONVERT);
        }
    }
    else if (data->state == DS18B20_STATE_CONVERTING)
    {
        uint32_t elapsed = HAL_GetTick() - data->conversion_start_tick;
        if (elapsed >= DS18B20_CONVERSION_TIME_MS)
        {
            start_reset(data, DS18B20_STATE_RESET_READ);
        }
    }
}

void ds18b20_interrupt(ds18b20_data_t *data)
{
    switch (data->state)
    {
        case DS18B20_STATE_RESET_CONVERT:
        {
            data->sensor_present = (data->rx_buffer[0] != DS18B20_NO_PRESENCE_BYTE);
            if (!data->sensor_present)
            {
                // No device — abort and wait for the next cycle
                data->last_read_tick = HAL_GetTick();
                data->state = DS18B20_STATE_IDLE;
                break;
            }

            // Sensor present — send Skip ROM + Convert T at data baud rate
            data->uart_set_baudrate(DS18B20_BAUD_DATA);
            encode_byte(DS18B20_CMD_SKIP_ROM,    &data->tx_buffer[0]);
            encode_byte(DS18B20_CMD_CONVERT_T,   &data->tx_buffer[8]);
            data->uart_transmit_receive(data->tx_buffer, data->rx_buffer, DS18B20_CMD_BUFFER_SIZE);
            data->state = DS18B20_STATE_CMD_CONVERT;
            break;
        }

        case DS18B20_STATE_CMD_CONVERT:
        {
            // Convert T sent — start the conversion timer and wait
            data->conversion_start_tick = HAL_GetTick();
            data->state = DS18B20_STATE_CONVERTING;
            break;
        }

        case DS18B20_STATE_RESET_READ:
        {
            data->sensor_present = (data->rx_buffer[0] != DS18B20_NO_PRESENCE_BYTE);
            if (!data->sensor_present)
            {
                data->last_read_tick = HAL_GetTick();
                data->state = DS18B20_STATE_IDLE;
                break;
            }

            // Sensor present — send Skip ROM + Read Scratchpad at data baud rate
            data->uart_set_baudrate(DS18B20_BAUD_DATA);
            encode_byte(DS18B20_CMD_SKIP_ROM,        &data->tx_buffer[0]);
            encode_byte(DS18B20_CMD_READ_SCRATCHPAD, &data->tx_buffer[8]);
            data->uart_transmit_receive(data->tx_buffer, data->rx_buffer, DS18B20_CMD_BUFFER_SIZE);
            data->state = DS18B20_STATE_CMD_READ;
            break;
        }

        case DS18B20_STATE_CMD_READ:
        {
            // Read Scratchpad command sent — clock out 2 temperature bytes (16 read slots)
            for (uint8_t i = 0; i < DS18B20_TEMP_BUFFER_SIZE; i++)
            {
                data->tx_buffer[i] = 0xFFU;  // 0xFF = read slot on the 1-Wire bus
            }
            data->uart_transmit_receive(data->tx_buffer, data->rx_buffer, DS18B20_TEMP_BUFFER_SIZE);
            data->state = DS18B20_STATE_READING;
            break;
        }

        case DS18B20_STATE_READING:
        {
            // Temperature bytes received — decode and return to idle
            data->temperature = decode_temperature(data->rx_buffer);
            data->last_read_tick = HAL_GetTick();
            data->state = DS18B20_STATE_IDLE;
            break;
        }

        default:
            break;
    }
}

int32_t ds18b20_get_temperature(ds18b20_data_t *data)
{
    return data->temperature;
}

bool ds18b20_is_sensor_present(ds18b20_data_t *data)
{
    return data->sensor_present;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/

/**
 * @brief Encode a single byte into 8 UART bytes for 1-Wire transmission
 * Each bit of @p byte becomes one UART byte: 0xFF for bit=1, 0x00 for bit=0.
 * Bits are sent LSB first as required by the DS18B20 protocol.
 * @param byte  Data byte to encode
 * @param buf   Output buffer, must be at least 8 bytes
 */
static void encode_byte(uint8_t byte, uint8_t *buf)
{
    for (uint8_t i = 0; i < 8U; i++)
    {
        buf[i] = (byte & (1U << i)) ? 0xFFU : 0x00U;
    }
}

/**
 * @brief Decode two DS18B20 scratchpad bytes from a 16-byte UART receive buffer
 * Each received UART byte corresponds to one bit: 0xFF means the bit was 1,
 * any other value means the bit was 0.  Bytes are received LSB first.
 * buf[0..7]  = temperature LSB bits (bit 0 .. bit 7)
 * buf[8..15] = temperature MSB bits (bit 0 .. bit 7 of MSB byte)
 * @param buf   16-byte UART receive buffer
 * @return Temperature in whole °C
 */
static int32_t decode_temperature(const uint8_t *buf)
{
    uint8_t lsb = 0;
    uint8_t msb = 0;

    for (uint8_t i = 0; i < 8U; i++)
    {
        if (buf[i]     == 0xFFU) { lsb |= (uint8_t)(1U << i); }
        if (buf[i + 8] == 0xFFU) { msb |= (uint8_t)(1U << i); }
    }

    // DS18B20 raw value: signed 16-bit, resolution 0.0625 °C per LSB
    // Integer °C = raw >> 4  (arithmetic right shift on int16_t)
    int16_t raw = (int16_t)((uint16_t)msb << 8 | (uint16_t)lsb);
    return (int32_t)(raw >> 4);
}

/**
 * @brief Start a 1-Wire reset pulse and transition to @p next_state on completion
 * Switches the UART to 9600 baud and transmits the reset byte.
 * @param data        Pointer to the driver data structure
 * @param next_state  State to enter while waiting for the interrupt
 */
static void start_reset(ds18b20_data_t *data, ds18b20_state_t next_state)
{
    data->uart_set_baudrate(DS18B20_BAUD_RESET);
    data->tx_buffer[0] = DS18B20_RESET_BYTE;
    data->uart_transmit_receive(data->tx_buffer, data->rx_buffer, 1U);
    data->state = next_state;
}
