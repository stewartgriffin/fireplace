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
#include <stddef.h>

/**************************************           DEFINES                    ******************************************/
#define PCF8574_READ_PERIOD_MS 50  // Period in milliseconds between reads
#define PCF8574_TIMER_MAX_MS 1000  // Maximum value for tick_timer before wrapping to 0

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void pcf8574_init(pcf8574_data_t * this,
		int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size),
		void (* write_complete_callback)(void))
{
	this->i2c_write = i2c_write;
	this->write_complete_callback = write_complete_callback;
	this->write_request = false;
	this->write_in_progress = false;
	this->tick_timer = 0;
	this->output_state = 0xFF;  // All pins high (default state)
}

void pcf8574_main(pcf8574_data_t * this)
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
}

void pcf8574_interrupt(pcf8574_data_t * this)
{
	// Check if this was a write operation before clearing the flag
	bool was_write = this->write_in_progress;

	if (this->write_in_progress == true)
	{
		this->write_in_progress = false;
	}

	// Call the registered callback if this was a write operation
	if (was_write && this->write_complete_callback != NULL)
	{
		this->write_complete_callback();
	}
}

void pcf8574_write(pcf8574_data_t * this, uint8_t data)
{
	this->output_state = data;
	this->write_request = true;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
