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
#include "stm32h5xx_hal.h"
#include "max6675.h"
#include "spi.h"

#include "ds3231.h"
#include "pcf8574.h"
#include "hd44780.h"
#include "i2c.h"
#include "gui.h"

/**************************************           DEFINES                    ******************************************/

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
max6675_data_t thermocouple;
int current_temperature = 0;

ds3231_data_t clock;
pcf8574_data_t gpio_expander;
hd44780_data_t display;

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size);
int clock_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
int clock_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
int gpio_expander_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
void display_update_pins(uint8_t d4_d7, bool rs, bool e);
void fireplace_update_gui(void);
void gpio_expander_write_complete_callback(void);
	
/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void fireplace_init(void)
{
	max6675_init(&thermocouple, thermocouple_spi_send_receive_wrapper);
	ds3231_init(&clock, clock_i2c_receive, clock_i2c_send);
	pcf8574_init(&gpio_expander, gpio_expander_i2c_send, gpio_expander_write_complete_callback);
	hd44780_init(&display, 4, 20, display_update_pins);
}

void fireplace_main(void)
{
	// All components now manage their own timing using HAL_GetTick()
	// Just call them as fast as possible - they will handle their own intervals
	max6675_main(&thermocouple);
	ds3231_main(&clock);
	pcf8574_main(&gpio_expander);
	hd44780_main(&display);

	fireplace_update_gui();

	if (HAL_GetTick() % 1000 == 0)
	{
		hd44780_write_buffer(&display,gui_get_screen_buffer());
	}


	current_temperature = max6675_get_temperature(&thermocouple);
}

void clock_i2c_interrupt(void)
{
	ds3231_interrupt(&clock);
}

void gpio_expander_i2c_interrupt(void)
{
	// pcf8574_interrupt will call the registered callback if this was a write operation
	pcf8574_interrupt(&gpio_expander);
}

void display_i2c_interrupt(void)
{
	hd44780_transfer_complete(&display);
}

void thermocouple_spi_interrupt(void)
{
	max6675_spi_irq_handler(&thermocouple);
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size)
{
	return HAL_SPI_TransmitReceive_IT(&hspi2, tx_buffer, rx_buffer, size);
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

void fireplace_update_gui(void)
{
	gui_set_time(ds3231_get_time(&clock));

	gui_set_fireplace(123);

	gui_set_ventilation(3);

	gui_set_ppm(20);

	gui_set_fireplace_temperature(122);
}

void gpio_expander_write_complete_callback(void)
{
	// Called when PCF8574 I2C write completes
	// This notifies HD44780 that the pin update is complete
	hd44780_transfer_complete(&display);
}
