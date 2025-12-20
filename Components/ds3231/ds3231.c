/***********************************************************************************************************************
 *
 *            File: ds3231.c
 *      Created on: Nov 13, 2025 9:48:57 PM
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "ds3231.h"

/**************************************           DEFINES                    ******************************************/
#define DS3231_CURRENT_TIME_START_ADDRESS 0x00
#define DS3231_CURRENT_TIME_LENGTH 7

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
void serialize_data(ds3231_data_t * this);
void deserialize_data(ds3231_data_t * this);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void ds3231_init(ds3231_data_t * this,
		int (* i2c_read)(uint8_t *data, uint16_t data_size),
		int (* i2c_write)(uint8_t *data, uint16_t data_size))
{
	this->i2c_read = i2c_read;
	this->i2c_write = i2c_write;
}

void ds3231_main(ds3231_data_t * this)
{
	if (this->time_update_request == true)
	{
		this->time_update_request = false;
		this->time_update_waiting_for_interrupt = true;
		serialize_data(this);
		this->i2c_write(this->tx_buffer, DS3231_CURRENT_TIME_LENGTH);
		return;
	}

	this->i2c_read(this->rx_buffer, DS3231_CURRENT_TIME_LENGTH);
}

void ds3231_interrupt(ds3231_data_t * this)
{
	if (this->time_update_waiting_for_interrupt == true)
	{
		this->time_update_waiting_for_interrupt = false;
		return;
	}

	if (this->time_update_request == true)
	{
		return;
	}

	deserialize_data(this);
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
void serialize_data(ds3231_data_t * this)
{
	this->tx_buffer[0x00] = ((this->current_time.second / 10) << 4) & 0x70;
	this->tx_buffer[0x00] |= ((this->current_time.second % 10) & 0x0F);

	this->tx_buffer[0x01] = ((this->current_time.minute / 10) << 4) & 0x70;
	this->tx_buffer[0x01] |= ((this->current_time.minute % 10) & 0x0F);

	this->tx_buffer[0x02] = ((this->current_time.hour / 10) << 4) & 0x30;
	this->tx_buffer[0x02] |= ((this->current_time.hour % 10) & 0x0F);

	this->tx_buffer[0x03] = (this->current_time.day_of_week & 0x07);

	this->tx_buffer[0x04] = ((this->current_time.day_of_month / 10) << 4) & 0x30;
	this->tx_buffer[0x04] |= ((this->current_time.day_of_month % 10) & 0x0F);

	this->tx_buffer[0x05] = ((this->current_time.month / 10) << 4) & 0x10;
	this->tx_buffer[0x05] |= ((this->current_time.month % 10) & 0x0F);

	this->tx_buffer[0x06] = ((this->current_time.year / 10) << 4) & 0xF0;
	this->tx_buffer[0x06] |= ((this->current_time.year % 10) & 0x0F);

	this->tx_buffer[0x0E] = this->control;
	this->tx_buffer[0x0F] = this->control_status;
	this->tx_buffer[0x10] = this->aging_offset;
	this->tx_buffer[0x11] = this->temp;
	this->tx_buffer[0x12] = (this->temp >> 8);
}

void deserialize_data(ds3231_data_t * this)
{
	this->current_time.second = (((this->rx_buffer[0x00] & 0x70) >> 4) * 10);
	this->current_time.second += ((this->rx_buffer[0x00] & 0x0F));

	this->current_time.minute = (((this->rx_buffer[0x01] & 0x70) >> 4) * 10);
	this->current_time.minute += ((this->rx_buffer[0x01] & 0x0F));

	this->current_time.hour = (((this->rx_buffer[0x02] & 0x30) >> 4) * 10);
	this->current_time.hour += ((this->rx_buffer[0x02] & 0x0F));

	this->current_time.day_of_week = (this->rx_buffer[0x03] & 0x07);

	this->current_time.day_of_month = (((this->rx_buffer[0x04] & 0x30) >> 4) * 10);
	this->current_time.day_of_month += ((this->rx_buffer[0x04] & 0x0F));

	this->current_time.month = (((this->rx_buffer[0x05] & 0x10) >> 4) * 10);
	this->current_time.month += ((this->rx_buffer[0x05] & 0x0F));

	this->current_time.year = (((this->rx_buffer[0x06] & 0xF0) >> 4) * 10);
	this->current_time.year += ((this->rx_buffer[0x06] & 0x0F));
}
