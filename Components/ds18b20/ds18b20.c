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
#define DS18B20_CONVERSION_TIME_MS  750U    // 12-bit resolution conversion time
#define DS18B20_SAMPLE_PERIOD_MS   2000U    // Interval between samples within a burst
#define DS18B20_BURST_PERIOD_MS   60000U    // Interval between bursts (one burst per minute)

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
static int32_t compute_median(int32_t *samples, uint8_t count);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/

void ds18b20_init(ds18b20_data_t *data,
                  int  (*uart_transmit_receive)(uint8_t *tx, uint8_t *rx, uint16_t size),
                  void (*uart_set_baudrate)(uint32_t baudrate))
{
    data->uart_transmit_receive = uart_transmit_receive;
    data->uart_set_baudrate = uart_set_baudrate;
    data->state = DS18B20_STATE_IDLE;
    data->sensor_present = false;
    data->conversion_start_tick = 0;
    data->last_read_tick = 0;

    // Burst state: start with sample_count = DS18B20_SAMPLE_COUNT (burst "done")
    // and last_burst_tick far in the past so the first burst begins immediately.
    data->sample_count = DS18B20_SAMPLE_COUNT;
    data->last_burst_tick = HAL_GetTick() - DS18B20_BURST_PERIOD_MS;

    data->filtered_head = 0;
    data->first_burst_done = false;

    for (uint8_t i = 0; i < DS18B20_SAMPLE_COUNT; i++)
    {
        data->raw_samples[i] = 0;
    }
    for (uint8_t i = 0; i < DS18B20_FILTERED_COUNT; i++)
    {
        data->filtered_samples[i] = 0;
    }
}

void ds18b20_main(ds18b20_data_t *data)
{
    if (data->state == DS18B20_STATE_IDLE)
    {
        if (data->sample_count < DS18B20_SAMPLE_COUNT)
        {
            // Burst in progress — wait DS18B20_SAMPLE_PERIOD_MS between samples
            uint32_t elapsed = HAL_GetTick() - data->last_read_tick;
            if (elapsed >= DS18B20_SAMPLE_PERIOD_MS)
            {
                start_reset(data, DS18B20_STATE_RESET_CONVERT);
            }
        }
        else
        {
            // Burst complete — wait DS18B20_BURST_PERIOD_MS before next burst
            uint32_t elapsed = HAL_GetTick() - data->last_burst_tick;
            if (elapsed >= DS18B20_BURST_PERIOD_MS)
            {
                data->sample_count = 0;
                start_reset(data, DS18B20_STATE_RESET_CONVERT);
            }
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
                // No device — abort the burst and wait for the next one
                data->sample_count = DS18B20_SAMPLE_COUNT;
                data->last_burst_tick = HAL_GetTick();
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
                // No device — abort the burst and wait for the next one
                data->sample_count = DS18B20_SAMPLE_COUNT;
                data->last_burst_tick = HAL_GetTick();
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
            // Store decoded sample in the burst buffer
            data->raw_samples[data->sample_count] = decode_temperature(data->rx_buffer);
            data->sample_count++;
            data->last_read_tick = HAL_GetTick();
            data->state = DS18B20_STATE_IDLE;

            if (data->sample_count == DS18B20_SAMPLE_COUNT)
            {
                // Burst complete — compute median and push into the circular filter buffer
                int32_t median = compute_median(data->raw_samples, DS18B20_SAMPLE_COUNT);

                if (!data->first_burst_done)
                {
                    // Seed the entire filter buffer with the first result
                    for (uint8_t i = 0; i < DS18B20_FILTERED_COUNT; i++)
                    {
                        data->filtered_samples[i] = median;
                    }
                    data->first_burst_done = true;
                }
                else
                {
                    data->filtered_samples[data->filtered_head] = median;
                    data->filtered_head = (uint8_t)((data->filtered_head + 1U) % DS18B20_FILTERED_COUNT);
                }

                data->last_burst_tick = HAL_GetTick();
            }
            break;
        }

        default:
            break;
    }
}

int32_t ds18b20_get_temperature(ds18b20_data_t *data)
{
    int32_t sum = 0;
    for (uint8_t i = 0; i < DS18B20_FILTERED_COUNT; i++)
    {
        sum += data->filtered_samples[i];
    }
    return sum / (int32_t)DS18B20_FILTERED_COUNT;
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

    // DS18B20 raw value: signed 16-bit, resolution 0.0625 °C per LSB (= 1/16 °C)
    // Return tenths of °C: (raw * 10 + 8) / 16  — rounds to nearest 0.1 °C
    int16_t raw = (int16_t)((uint16_t)msb << 8 | (uint16_t)lsb);
    return ((int32_t)raw * 10 + 8) / 16;
}

/**
 * @brief Compute the median of an int32_t array using insertion sort on a local copy
 * For an even-length array, returns the upper-middle element (index count/2).
 * @param samples  Array of @p count values (not modified)
 * @param count    Number of elements (must be <= DS18B20_SAMPLE_COUNT)
 * @return Median value
 */
static int32_t compute_median(int32_t *samples, uint8_t count)
{
    int32_t sorted[DS18B20_SAMPLE_COUNT];
    for (uint8_t i = 0; i < count; i++)
    {
        sorted[i] = samples[i];
    }

    // Insertion sort
    for (uint8_t i = 1; i < count; i++)
    {
        int32_t key = sorted[i];
        int8_t  j   = (int8_t)(i - 1);
        while (j >= 0 && sorted[j] > key)
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    return sorted[count / 2U];
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
