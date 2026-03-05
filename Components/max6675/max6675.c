/***********************************************************************************************************************
 *
 *            File: max6675.c
 *      Created on: Oct 16, 2025 10:51:55 PM
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "max6675.h"
#include "stm32h5xx_hal.h"
#include "stdint.h"

/**************************************           DEFINES                    ******************************************/
#define MAX6675_SPI_TRANSFER_LENGTH 1
#define MAX6675_READ_PERIOD_MS 1000  // Read temperature every 1000ms
#define MAX6675_HYSTERESIS_DEG 2    // Degrees C required to reverse direction

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
void max6675_serialize_spi_data(max6675_data_t * this);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
void max6675_init(max6675_data_t * this,
					int (*spi_start_transfer)(uint8_t *tx_buffer, uint8_t * rx_buffer, uint16_t size))
{
	this->spi_start_transfer = spi_start_transfer;
	this->tick_timer = 0;
	this->last_tick = HAL_GetTick();
	this->transfer_in_progress = false;
	this->temperature = 0;
	this->filtered_temperature = 0;
	this->direction = MAX6675_DIRECTION_NONE;
}

void max6675_main(max6675_data_t * this)
{
	// Skip if transfer is in progress
	if (this->transfer_in_progress)
	{
		return;
	}

	// Calculate elapsed time since last call
	uint32_t current_tick = HAL_GetTick();
	uint32_t elapsed_ms = current_tick - this->last_tick;
	this->last_tick = current_tick;

	// Increment tick timer
	this->tick_timer += elapsed_ms;

	// Read temperature at specified period
	if (this->tick_timer >= MAX6675_READ_PERIOD_MS)
	{
		this->tick_timer = 0;
		this->transfer_in_progress = true;
		this->spi_start_transfer(this->tx_buffer, this->rx_buffer, MAX6675_SPI_TRANSFER_LENGTH);
	}
}

int max6675_get_temperature(max6675_data_t * this)
{
	return this->filtered_temperature;
}

void max6675_spi_irq_handler(max6675_data_t * this)
{
	this->transfer_in_progress = false;
	max6675_serialize_spi_data(this);
}

void max6675_serialize_spi_data(max6675_data_t * this)
{
	uint32_t raw = ((uint32_t)this->rx_buffer[0] >> 3);
	raw |= ((uint32_t)this->rx_buffer[1] << 5);
	raw = raw / 4;
	this->conection_open = ((this->rx_buffer[0] & 0x04) == 0x04);
	this->temperature = raw;

	if (this->direction == MAX6675_DIRECTION_NONE)
	{
		if (raw > this->filtered_temperature)
		{
			this->direction = MAX6675_DIRECTION_RISING;
			this->filtered_temperature = raw;
		}
		else if (raw < this->filtered_temperature)
		{
			this->direction = MAX6675_DIRECTION_FALLING;
			this->filtered_temperature = raw;
		}
	}
	else if (this->direction == MAX6675_DIRECTION_RISING)
	{
		if (raw >= this->filtered_temperature)
		{
			this->filtered_temperature = raw;
		}
		else if ((this->filtered_temperature - raw) >= MAX6675_HYSTERESIS_DEG)
		{
			this->direction = MAX6675_DIRECTION_FALLING;
			this->filtered_temperature = raw;
		}
	}
	else // MAX6675_DIRECTION_FALLING
	{
		if (raw <= this->filtered_temperature)
		{
			this->filtered_temperature = raw;
		}
		else if ((raw - this->filtered_temperature) >= MAX6675_HYSTERESIS_DEG)
		{
			this->direction = MAX6675_DIRECTION_RISING;
			this->filtered_temperature = raw;
		}
	}
}
