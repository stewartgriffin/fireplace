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
#include "stdint.h"

/**************************************           DEFINES                    ******************************************/
#define MAX6675_SPI_TRANSFER_LENGTH 1

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
}

void max6675_main(max6675_data_t * this)
{
	this->spi_start_transfer(this->tx_buffer, this->rx_buffer, MAX6675_SPI_TRANSFER_LENGTH);
	max6675_serialize_spi_data(this);
}

int max6675_get_temperature(max6675_data_t * this)
{
	return this->temperature;
}

void max6675_serialize_spi_data(max6675_data_t * this)
{
	this->temperature = ((uint32_t)this->rx_buffer[0] >> 3);
	this->temperature |= ((uint32_t)this->rx_buffer[1] << 5);
	this->temperature = this->temperature / 4;
	this->conection_open = ((this->rx_buffer[0] & 0x04) == 0x04);
}
