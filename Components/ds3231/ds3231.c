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

#define DS3231_ALARM1_START_ADDRESS 0x07
#define DS3231_ALARM1_LENGTH 4

#define DS3231_ALARM2_START_ADDRESS 0x0B
#define DS3231_ALARM2_LENGTH 3

#define DS3231_STATUS_START_ADDRESS 0x0E
#define DS3231_STATUS_LENGTH 5  // Control (0x0E), Control/Status (0x0F), Aging Offset (0x10), Temp MSB (0x11), Temp LSB (0x12)

#define DS3231_STATUS_REG_OSF_BIT 0x80  // Oscillator Stop Flag in status register (0x0F)

#define DS3231_TIME_READ_PERIOD_MS 100  // Period in milliseconds between current time reads
#define DS3231_STATUS_READ_PERIOD_MS 1000  // Period in milliseconds between status reads
#define DS3231_TIMER_MAX_MS 1000  // Maximum value for tick_timer before wrapping to 0

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static void serialize_current_time(ds3231_data_t * this);
static void deserialize_current_time(ds3231_data_t * this);
static void serialize_alarm1(ds3231_data_t * this);
static void deserialize_alarm1(ds3231_data_t * this);
static void serialize_alarm2(ds3231_data_t * this);
static void deserialize_alarm2(ds3231_data_t * this);
static void serialize_status(ds3231_data_t * this);
static void deserialize_status(ds3231_data_t * this);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void ds3231_init(ds3231_data_t * this,
		int (* i2c_read)(uint8_t mem_addr, uint8_t *data, uint16_t data_size),
		int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size))
{
	this->i2c_read = i2c_read;
	this->i2c_write = i2c_write;
	this->oscillator_not_started = false;
	this->oscillator_check_pending = true;
	this->time_read_in_progress = false;
	this->status_read_in_progress = false;
	this->alarm1_update_request = false;
	this->alarm1_update_waiting_for_interrupt = false;
	this->alarm2_update_request = false;
	this->alarm2_update_waiting_for_interrupt = false;
	this->alarm1_read_request = false;
	this->alarm1_read_in_progress = false;
	this->alarm2_read_request = false;
	this->alarm2_read_in_progress = false;
	this->tick_timer = 0;

	// Request read of status registers to check oscillator stop flag
	this->i2c_read(DS3231_STATUS_START_ADDRESS, this->rx_buffer, DS3231_STATUS_LENGTH);
}

void ds3231_main(ds3231_data_t * this, uint32_t call_period_ms)
{
	// Check if we need to wake up the oscillator
	if (this->oscillator_not_started == true)
	{
		// Wake up DS3231 by writing to it (first-time power-on initialization)
		// This is required to start the oscillator on first VBAT application
		// According to DS3231 datasheet, oscillator won't start until VCC exceeds VPF
		// threshold OR a valid I2C write occurs
		uint8_t dummy_data = 0x00;
		this->tx_buffer[0] = dummy_data;
		this->i2c_write(DS3231_CURRENT_TIME_START_ADDRESS, this->tx_buffer, 1);
		this->oscillator_not_started = false;
		return;
	}

	if (this->time_update_request == true)
	{
		this->time_update_request = false;
		this->time_update_waiting_for_interrupt = true;
		serialize_current_time(this);
		this->i2c_write(DS3231_CURRENT_TIME_START_ADDRESS, this->tx_buffer, DS3231_CURRENT_TIME_LENGTH);
		return;
	}

	if (this->alarm1_update_request == true)
	{
		this->alarm1_update_request = false;
		this->alarm1_update_waiting_for_interrupt = true;
		serialize_alarm1(this);
		this->i2c_write(DS3231_ALARM1_START_ADDRESS, this->tx_buffer, DS3231_ALARM1_LENGTH);
		return;
	}

	if (this->alarm2_update_request == true)
	{
		this->alarm2_update_request = false;
		this->alarm2_update_waiting_for_interrupt = true;
		serialize_alarm2(this);
		this->i2c_write(DS3231_ALARM2_START_ADDRESS, this->tx_buffer, DS3231_ALARM2_LENGTH);
		return;
	}

	if (this->alarm1_read_request == true)
	{
		this->alarm1_read_request = false;
		this->alarm1_read_in_progress = true;
		this->i2c_read(DS3231_ALARM1_START_ADDRESS, this->rx_buffer, DS3231_ALARM1_LENGTH);
		return;
	}

	if (this->alarm2_read_request == true)
	{
		this->alarm2_read_request = false;
		this->alarm2_read_in_progress = true;
		this->i2c_read(DS3231_ALARM2_START_ADDRESS, this->rx_buffer, DS3231_ALARM2_LENGTH);
		return;
	}

	// Read current time at specified period
	if (this->tick_timer % DS3231_TIME_READ_PERIOD_MS == 0)
	{
		this->time_read_in_progress = true;
		this->i2c_read(DS3231_CURRENT_TIME_START_ADDRESS, this->rx_buffer, DS3231_CURRENT_TIME_LENGTH);
	}

	// Read status registers at specified period
	if (this->tick_timer % DS3231_STATUS_READ_PERIOD_MS == 0)
	{
		this->status_read_in_progress = true;
		this->i2c_read(DS3231_STATUS_START_ADDRESS, this->rx_buffer, DS3231_STATUS_LENGTH);
	}

	// Increment tick timer
	this->tick_timer += call_period_ms;

	// Wrap timer to prevent overflow
	if (this->tick_timer >= DS3231_TIMER_MAX_MS)
	{
		this->tick_timer = 0;
	}
}

void ds3231_interrupt(ds3231_data_t * this)
{
	// Check if this interrupt is from initial oscillator check
	if (this->oscillator_check_pending == true)
	{
		this->oscillator_check_pending = false;
		// Deserialize status registers
		deserialize_status(this);
		// Check OSF bit in Control/Status register (0x0F)
		// OSF bit is bit 7 (0x80) of control_status register
		if (this->control_status & DS3231_STATUS_REG_OSF_BIT)
		{
			this->oscillator_not_started = true;
		}
	}
	else if (this->time_update_waiting_for_interrupt == true)
	{
		this->time_update_waiting_for_interrupt = false;
	}
	else if (this->alarm1_update_waiting_for_interrupt == true)
	{
		this->alarm1_update_waiting_for_interrupt = false;
	}
	else if (this->alarm2_update_waiting_for_interrupt == true)
	{
		this->alarm2_update_waiting_for_interrupt = false;
	}
	else if (this->time_update_request == true)
	{
		// Request still pending, do nothing
	}
	else if (this->alarm1_update_request == true)
	{
		// Request still pending, do nothing
	}
	else if (this->alarm2_update_request == true)
	{
		// Request still pending, do nothing
	}
	else if (this->alarm1_read_request == true)
	{
		// Request still pending, do nothing
	}
	else if (this->alarm2_read_request == true)
	{
		// Request still pending, do nothing
	}
	else if (this->status_read_in_progress == true)
	{
		this->status_read_in_progress = false;
		deserialize_status(this);
	}
	else if (this->time_read_in_progress == true)
	{
		this->time_read_in_progress = false;
		deserialize_current_time(this);
	}
	else if (this->alarm1_read_in_progress == true)
	{
		this->alarm1_read_in_progress = false;
		deserialize_alarm1(this);
	}
	else if (this->alarm2_read_in_progress == true)
	{
		this->alarm2_read_in_progress = false;
		deserialize_alarm2(this);
	}
}

void ds3231_set_time(ds3231_data_t * this, time_data_t * new_time)
{
	// Update the current time in the structure
	this->current_time.second = new_time->second;
	this->current_time.minute = new_time->minute;
	this->current_time.hour = new_time->hour;
	this->current_time.day_of_week = new_time->day_of_week;
	this->current_time.day_of_month = new_time->day_of_month;
	this->current_time.month = new_time->month;
	this->current_time.year = new_time->year;

	// Set the flag to request time update on next ds3231_main() call
	this->time_update_request = true;
}

void ds3231_set_alarm1(ds3231_data_t * this, uint8_t seconds, uint8_t minutes, uint8_t hours, uint8_t day_date)
{
	// Update alarm 1 registers in the structure
	this->alarm1_seconds = seconds;
	this->alarm1_minutes = minutes;
	this->alarm1_hours = hours;
	this->alarm1_day_date = day_date;

	// Set the flag to request alarm1 update on next ds3231_main() call
	this->alarm1_update_request = true;
}

void ds3231_set_alarm2(ds3231_data_t * this, uint8_t minutes, uint8_t hours, uint8_t day_date)
{
	// Update alarm 2 registers in the structure
	this->alarm2_minutes = minutes;
	this->alarm2_hours = hours;
	this->alarm2_day_date = day_date;

	// Set the flag to request alarm2 update on next ds3231_main() call
	this->alarm2_update_request = true;
}

void ds3231_read_alarm1(ds3231_data_t * this)
{
	// Set the flag to request alarm1 read on next ds3231_main() call
	this->alarm1_read_request = true;
}

void ds3231_read_alarm2(ds3231_data_t * this)
{
	// Set the flag to request alarm2 read on next ds3231_main() call
	this->alarm2_read_request = true;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
static void serialize_current_time(ds3231_data_t * this)
{
	// Time and Date registers (0x00-0x06)
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
}

static void deserialize_current_time(ds3231_data_t * this)
{
	// Time and Date registers (0x00-0x06)
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

static void serialize_alarm1(ds3231_data_t * this)
{
	// Alarm 1 registers (0x07-0x0A)
	this->tx_buffer[0x00] = this->alarm1_seconds;
	this->tx_buffer[0x01] = this->alarm1_minutes;
	this->tx_buffer[0x02] = this->alarm1_hours;
	this->tx_buffer[0x03] = this->alarm1_day_date;
}

static void deserialize_alarm1(ds3231_data_t * this)
{
	// Alarm 1 registers (0x07-0x0A)
	this->alarm1_seconds = this->rx_buffer[0x00];
	this->alarm1_minutes = this->rx_buffer[0x01];
	this->alarm1_hours = this->rx_buffer[0x02];
	this->alarm1_day_date = this->rx_buffer[0x03];
}

static void serialize_alarm2(ds3231_data_t * this)
{
	// Alarm 2 registers (0x0B-0x0D)
	this->tx_buffer[0x00] = this->alarm2_minutes;
	this->tx_buffer[0x01] = this->alarm2_hours;
	this->tx_buffer[0x02] = this->alarm2_day_date;
}

static void deserialize_alarm2(ds3231_data_t * this)
{
	// Alarm 2 registers (0x0B-0x0D)
	this->alarm2_minutes = this->rx_buffer[0x00];
	this->alarm2_hours = this->rx_buffer[0x01];
	this->alarm2_day_date = this->rx_buffer[0x02];
}

static void deserialize_status(ds3231_data_t * this)
{
	// Control register (0x0E)
	this->control = this->rx_buffer[0x00];

	// Control/Status register (0x0F)
	this->control_status = this->rx_buffer[0x01];

	// Aging offset register (0x10)
	this->aging_offset = this->rx_buffer[0x02];

	// Temperature registers (0x11-0x12) - MSB first, then LSB
	this->temp = (this->rx_buffer[0x03] << 8) | this->rx_buffer[0x04];
}
