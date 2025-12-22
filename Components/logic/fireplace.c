/***********************************************************************************************************************
 *
 *            File: fireplace.c
 *      Created on: Oct 25, 2025 9:55:48 PM
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"
#include "max6675.h"
#include "spi.h"

#include "ds3231.h"
#include "pcf8574.h"
#include "hd44780.h"
#include "i2c.h"

/**************************************           DEFINES                    ******************************************/

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
int main_function_timer = 0;
max6675_data_t thermocouple;
int current_temperature = 0;

static ds3231_data_t clock;
static pcf8574_data_t gpio_expander;
static hd44780_data_t display;

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size);
int clock_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
int clock_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
int gpio_expander_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
int gpio_expander_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
void display_update_pins(uint8_t d4_d7, bool rs, bool e);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void fireplace_init(void)
{
	max6675_init(&thermocouple, thermocouple_spi_send_receive_wrapper);
	ds3231_init(&clock, clock_i2c_receive, clock_i2c_send);
	pcf8574_init(&gpio_expander, gpio_expander_i2c_receive, gpio_expander_i2c_send);
	hd44780_init(&display, 4, 20, display_update_pins);
}

void fireplace_main(void)
{
	if (main_function_timer % 1000 == 0)
	{
		max6675_main(&thermocouple);
	}

	if (main_function_timer % 1 == 0)
	{
		ds3231_main(&clock, 1);
		pcf8574_main(&gpio_expander, 1);
		hd44780_main(&display, 1);
	}

	current_temperature = max6675_get_temperature(&thermocouple);
	main_function_timer++;

	if (main_function_timer == 1000)
	{
		main_function_timer = 0;
	}
}

void clock_i2c_interrupt(void)
{
	ds3231_interrupt(&clock);
}

void gpio_expander_i2c_interrupt(void)
{
	pcf8574_interrupt(&gpio_expander);
}

void display_i2c_interrupt(void)
{
	hd44780_transfer_complete(&display);
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size)
{
	return HAL_SPI_TransmitReceive(&hspi2, tx_buffer, rx_buffer, size, 1000);
}

int clock_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Mem_Write_IT(&hi2c2, 0x68 << 1, mem_addr, 1, data, data_size);
}

int clock_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Mem_Read_IT(&hi2c2, 0x68 << 1, mem_addr, 1, data, data_size);
}

int gpio_expander_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Master_Transmit_IT(&hi2c3, 0x27 << 1, data, data_size);
}

int gpio_expander_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Master_Receive_IT(&hi2c3, 0x27 << 1, data, data_size);
}

void display_update_pins(uint8_t d4_d7, bool rs, bool e)
{
	// Pack HD44780 control signals into PCF8574 byte
	// Bit mapping: [D7 D6 D5 D4 BL E RW RS]
	uint8_t output = 0;

	// Data lines D4-D7 (bits 4-7)
	output |= (d4_d7 & 0x0F) << 4;

	// RS - Register Select (bit 0)
	if (rs)
	{
		output |= 0x01;
	}

	// RW - Read/Write (bit 1) - always 0 for write
	// output |= 0x00;

	// E - Enable (bit 2)
	if (e)
	{
		output |= 0x04;
	}

	// BL - Backlight (bit 3) - always on
	output |= 0x08;

	// Write to PCF8574
	pcf8574_write(&gpio_expander, output);
}
