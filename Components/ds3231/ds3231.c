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
#include "stm32h5xx_hal.h"

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
	this->osf_clear_pending = false;
	this->time_read_in_progress = false;
	this->time_read_paused = false;
	this->status_read_in_progress = true;  // Status read initiated below
	this->alarm1_update_request = false;
	this->alarm1_update_waiting_for_interrupt = false;
	this->alarm2_update_request = false;
	this->alarm2_update_waiting_for_interrupt = false;
	this->alarm1_read_request = false;
	this->alarm1_read_in_progress = false;
	this->alarm2_read_request = false;
	this->alarm2_read_in_progress = false;
	this->tick_timer = 0;
	this->last_tick = HAL_GetTick();

	// Start in PREINIT state - query status register to check oscillator state
	this->state = DS3231_STATE_PREINIT;
	this->i2c_read(DS3231_STATUS_START_ADDRESS, this->rx_buffer, DS3231_STATUS_LENGTH);
}

void ds3231_main(ds3231_data_t * this)
{
	switch (this->state)
	{
		case DS3231_STATE_PREINIT:
			// In PREINIT, oscillator state is unknown
			// Wait for initial status read to complete (handled in interrupt)
			// Don't start any operations until we know if OSF needs clearing
			break;

		case DS3231_STATE_CLEARING_OSF:
			// Clear the OSF (Oscillator Stop Flag) in Control/Status register (0x0F, bit 7)
			// OSF must be explicitly cleared by writing 0 to bit 7
			// Also preserve other bits in the control/status register
			this->tx_buffer[0] = this->control;         // Control register (0x0E)
			this->tx_buffer[1] = this->control_status & 0x7F;  // Clear OSF bit (bit 7)
			this->i2c_write(DS3231_STATUS_START_ADDRESS, this->tx_buffer, 2);
			this->osf_clear_pending = true;
			// Stay in CLEARING_OSF state (transition to IDLE happens in interrupt)
			break;

		case DS3231_STATE_IDLE:
			// Ready for operations - check for pending requests

			if (this->time_update_request == true)
			{
				this->time_update_request = false;
				this->time_update_waiting_for_interrupt = true;
				serialize_current_time(this);
				this->i2c_write(DS3231_CURRENT_TIME_START_ADDRESS, this->tx_buffer, DS3231_CURRENT_TIME_LENGTH);
				this->state = DS3231_STATE_RUNNING;
				return;
			}

			if (this->alarm1_update_request == true)
			{
				this->alarm1_update_request = false;
				this->alarm1_update_waiting_for_interrupt = true;
				serialize_alarm1(this);
				this->i2c_write(DS3231_ALARM1_START_ADDRESS, this->tx_buffer, DS3231_ALARM1_LENGTH);
				this->state = DS3231_STATE_RUNNING;
				return;
			}

			if (this->alarm2_update_request == true)
			{
				this->alarm2_update_request = false;
				this->alarm2_update_waiting_for_interrupt = true;
				serialize_alarm2(this);
				this->i2c_write(DS3231_ALARM2_START_ADDRESS, this->tx_buffer, DS3231_ALARM2_LENGTH);
				this->state = DS3231_STATE_RUNNING;
				return;
			}

			if (this->alarm1_read_request == true)
			{
				this->alarm1_read_request = false;
				this->alarm1_read_in_progress = true;
				this->i2c_read(DS3231_ALARM1_START_ADDRESS, this->rx_buffer, DS3231_ALARM1_LENGTH);
				this->state = DS3231_STATE_RUNNING;
				return;
			}

			if (this->alarm2_read_request == true)
			{
				this->alarm2_read_request = false;
				this->alarm2_read_in_progress = true;
				this->i2c_read(DS3231_ALARM2_START_ADDRESS, this->rx_buffer, DS3231_ALARM2_LENGTH);
				this->state = DS3231_STATE_RUNNING;
				return;
			}

			// Calculate elapsed time since last call
			uint32_t current_tick = HAL_GetTick();
			uint32_t elapsed_ms = current_tick - this->last_tick;
			this->last_tick = current_tick;

			// Read current time at specified period (unless paused)
			if (!this->time_read_paused && (this->tick_timer % DS3231_TIME_READ_PERIOD_MS == 0))
			{
				this->time_read_in_progress = true;
				this->i2c_read(DS3231_CURRENT_TIME_START_ADDRESS, this->rx_buffer, DS3231_CURRENT_TIME_LENGTH);
				this->state = DS3231_STATE_RUNNING;
			}

			// Read status registers at specified period (offset by 50ms to avoid collision)
			if ((this->tick_timer + 50) % DS3231_STATUS_READ_PERIOD_MS == 0)
			{
				this->status_read_in_progress = true;
				this->i2c_read(DS3231_STATUS_START_ADDRESS, this->rx_buffer, DS3231_STATUS_LENGTH);
				this->state = DS3231_STATE_RUNNING;
			}

			// Increment tick timer
			this->tick_timer += elapsed_ms;

			// Wrap timer to prevent overflow
			if (this->tick_timer >= DS3231_TIMER_MAX_MS)
			{
				this->tick_timer = 0;
			}
			break;

		case DS3231_STATE_RUNNING:
			// Active transmission in progress - wait for interrupt
			// Don't start any new operations
			break;
	}
}

void ds3231_interrupt(ds3231_data_t * this)
{
	switch (this->state)
	{
		case DS3231_STATE_PREINIT:
			// Initial status read completed - check OSF flag
			this->status_read_in_progress = false;
			deserialize_status(this);

			// Check OSF bit in Control/Status register (0x0F)
			// OSF bit is bit 7 (0x80) of control_status register
			if (this->control_status & DS3231_STATUS_REG_OSF_BIT)
			{
				// OSF is set - need to clear it (virgin DS3231)
				this->state = DS3231_STATE_CLEARING_OSF;
			}
			else
			{
				// OSF not set - oscillator is running, go to IDLE
				this->state = DS3231_STATE_IDLE;
			}
			break;

		case DS3231_STATE_CLEARING_OSF:
			// OSF clear write completed
			if (this->osf_clear_pending)
			{
				this->osf_clear_pending = false;

				// RTC was uninitialized - set a known default date/time (26.02.2026 00:00:00)
				this->current_time.second       = 0;
				this->current_time.minute       = 0;
				this->current_time.hour         = 0;
				this->current_time.day_of_week  = 4;  // Thursday
				this->current_time.day_of_month = 26;
				this->current_time.month        = 2;
				this->current_time.year         = 26; // 2026
				this->time_update_request       = true;

				// Transition to IDLE - initialization complete
				this->state = DS3231_STATE_IDLE;
			}
			break;

		case DS3231_STATE_IDLE:
			// Should not receive interrupts in IDLE state
			// (only happens if there's a race condition)
			break;

		case DS3231_STATE_RUNNING:
			// Handle various interrupt sources and return to IDLE
			if (this->time_update_waiting_for_interrupt == true)
			{
				this->time_update_waiting_for_interrupt = false;
				this->state = DS3231_STATE_IDLE;
			}
			else if (this->alarm1_update_waiting_for_interrupt == true)
			{
				this->alarm1_update_waiting_for_interrupt = false;
				this->state = DS3231_STATE_IDLE;
			}
			else if (this->alarm2_update_waiting_for_interrupt == true)
			{
				this->alarm2_update_waiting_for_interrupt = false;
				this->state = DS3231_STATE_IDLE;
			}
			else if (this->status_read_in_progress == true)
			{
				this->status_read_in_progress = false;
				deserialize_status(this);
				this->state = DS3231_STATE_IDLE;
			}
			else if (this->time_read_in_progress == true)
			{
				this->time_read_in_progress = false;
				deserialize_current_time(this);
				this->state = DS3231_STATE_IDLE;
			}
			else if (this->alarm1_read_in_progress == true)
			{
				this->alarm1_read_in_progress = false;
				deserialize_alarm1(this);
				this->state = DS3231_STATE_IDLE;
			}
			else if (this->alarm2_read_in_progress == true)
			{
				this->alarm2_read_in_progress = false;
				deserialize_alarm2(this);
				this->state = DS3231_STATE_IDLE;
			}
			break;
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

	// Resume time reading (in case it was paused during editing)
	this->time_read_paused = false;
}

void ds3231_adjust_time(ds3231_data_t * this, ds3231_time_param_t param, ds3231_adjust_dir_t dir)
{
	switch (param)
	{
		case DS3231_PARAM_HOUR:
			if (dir == DS3231_ADJUST_UP)
			{
				this->current_time.hour++;
				if (this->current_time.hour > 23)
				{
					this->current_time.hour = 0;
				}
			}
			else
			{
				if (this->current_time.hour == 0)
				{
					this->current_time.hour = 23;
				}
				else
				{
					this->current_time.hour--;
				}
			}
			break;

		case DS3231_PARAM_MINUTE:
			if (dir == DS3231_ADJUST_UP)
			{
				this->current_time.minute++;
				if (this->current_time.minute > 59)
				{
					this->current_time.minute = 0;
				}
			}
			else
			{
				if (this->current_time.minute == 0)
				{
					this->current_time.minute = 59;
				}
				else
				{
					this->current_time.minute--;
				}
			}
			break;

		case DS3231_PARAM_SECOND:
			if (dir == DS3231_ADJUST_UP)
			{
				this->current_time.second++;
				if (this->current_time.second > 59)
				{
					this->current_time.second = 0;
				}
			}
			else
			{
				if (this->current_time.second == 0)
				{
					this->current_time.second = 59;
				}
				else
				{
					this->current_time.second--;
				}
			}
			break;

		case DS3231_PARAM_DAY:
			if (dir == DS3231_ADJUST_UP)
			{
				this->current_time.day_of_month++;
				if (this->current_time.day_of_month > 31)
				{
					this->current_time.day_of_month = 1;
				}
			}
			else
			{
				if (this->current_time.day_of_month == 1)
				{
					this->current_time.day_of_month = 31;
				}
				else
				{
					this->current_time.day_of_month--;
				}
			}
			break;

		case DS3231_PARAM_MONTH:
			if (dir == DS3231_ADJUST_UP)
			{
				this->current_time.month++;
				if (this->current_time.month > 12)
				{
					this->current_time.month = 1;
				}
			}
			else
			{
				if (this->current_time.month == 1)
				{
					this->current_time.month = 12;
				}
				else
				{
					this->current_time.month--;
				}
			}
			break;

		case DS3231_PARAM_YEAR:
			if (dir == DS3231_ADJUST_UP)
			{
				this->current_time.year++;
				if (this->current_time.year > 99)
				{
					this->current_time.year = 0;
				}
			}
			else
			{
				if (this->current_time.year == 0)
				{
					this->current_time.year = 99;
				}
				else
				{
					this->current_time.year--;
				}
			}
			break;

		default:
			break;
	}
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

time_data_t * ds3231_get_time(ds3231_data_t * this)
{
	// Return pointer to current time structure
	return &this->current_time;
}

void ds3231_pause_time_read(ds3231_data_t * this)
{
	// Pause periodic time reading from DS3231
	this->time_read_paused = true;
}

void ds3231_resume_time_read(ds3231_data_t * this)
{
	// Resume periodic time reading from DS3231
	this->time_read_paused = false;
}

void ds3231_error(ds3231_data_t * this)
{
	this->state = DS3231_STATE_IDLE;
	this->time_read_in_progress = false;
	this->status_read_in_progress = false;
	this->time_update_waiting_for_interrupt = false;
	this->alarm1_update_waiting_for_interrupt = false;
	this->alarm2_update_waiting_for_interrupt = false;
	this->alarm1_read_in_progress = false;
	this->alarm2_read_in_progress = false;
	this->osf_clear_pending = false;
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
