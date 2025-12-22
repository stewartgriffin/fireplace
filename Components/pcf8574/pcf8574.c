/***********************************************************************************************************************
 *
 *            File: pcf8574.c
 *      Created on: Dec 21, 2025
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "pcf8574.h"

/**************************************           DEFINES                    ******************************************/
#define PCF8574_READ_PERIOD_MS 50  // Period in milliseconds between reads
#define PCF8574_TIMER_MAX_MS 1000  // Maximum value for tick_timer before wrapping to 0

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void pcf8574_init(pcf8574_data_t * this,
		int (* i2c_read)(uint8_t mem_addr, uint8_t *data, uint16_t data_size),
		int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size))
{
	this->i2c_read = i2c_read;
	this->i2c_write = i2c_write;
	this->write_request = false;
	this->write_in_progress = false;
	this->read_request = false;
	this->read_in_progress = false;
	this->tick_timer = 0;
	this->output_state = 0xFF;  // All pins high (default state)
	this->input_state = 0xFF;   // All pins high (default state)

	// Initialize by reading current state
	this->read_in_progress = true;
	this->i2c_read(0x00, this->rx_buffer, 1);
}

void pcf8574_main(pcf8574_data_t * this, uint32_t call_period_ms)
{
	// Handle write request
	if (this->write_request == true)
	{
		this->write_request = false;
		this->write_in_progress = true;
		this->tx_buffer[0] = this->output_state;
		this->i2c_write(0x00, this->tx_buffer, 1);
		return;
	}

	// Handle read request
	if (this->read_request == true)
	{
		this->read_request = false;
		this->read_in_progress = true;
		this->i2c_read(0x00, this->rx_buffer, 1);
		return;
	}

	// Periodic read at specified period
	if (this->tick_timer % PCF8574_READ_PERIOD_MS == 0)
	{
		this->read_in_progress = true;
		this->i2c_read(0x00, this->rx_buffer, 1);
	}

	// Increment tick timer
	this->tick_timer += call_period_ms;

	// Wrap timer to prevent overflow
	if (this->tick_timer >= PCF8574_TIMER_MAX_MS)
	{
		this->tick_timer = 0;
	}
}

void pcf8574_interrupt(pcf8574_data_t * this)
{
	if (this->write_in_progress == true)
	{
		this->write_in_progress = false;
	}
	else if (this->read_in_progress == true)
	{
		this->read_in_progress = false;
		this->input_state = this->rx_buffer[0];
	}
}

void pcf8574_write(pcf8574_data_t * this, uint8_t data)
{
	this->output_state = data;
	this->write_request = true;
}

void pcf8574_read(pcf8574_data_t * this)
{
	this->read_request = true;
}

void pcf8574_set_pin(pcf8574_data_t * this, uint8_t pin, bool state)
{
	if (pin > 7)
	{
		return;
	}

	if (state)
	{
		// Set pin high
		this->output_state |= (1 << pin);
	}
	else
	{
		// Set pin low
		this->output_state &= ~(1 << pin);
	}

	this->write_request = true;
}

bool pcf8574_get_pin(pcf8574_data_t * this, uint8_t pin)
{
	if (pin > 7)
	{
		return false;
	}

	return (this->input_state & (1 << pin)) != 0;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
