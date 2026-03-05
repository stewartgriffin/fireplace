/***********************************************************************************************************************
 *
 *            File: ds3231.h
 *      Created on: Nov 13, 2025 9:49:07 PM
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef SRC_DRIVERS_DS3231_DS3231_H_
#define SRC_DRIVERS_DS3231_DS3231_H_

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/
typedef enum
{
	DS3231_PARAM_SECOND,
	DS3231_PARAM_MINUTE,
	DS3231_PARAM_HOUR,
	DS3231_PARAM_DAY,
	DS3231_PARAM_MONTH,
	DS3231_PARAM_YEAR
}ds3231_time_param_t;

typedef enum
{
	DS3231_ADJUST_UP,
	DS3231_ADJUST_DOWN
}ds3231_adjust_dir_t;

typedef enum
{
	DS3231_STATE_PREINIT,       // Oscillator state unknown, query status register
	DS3231_STATE_CLEARING_OSF,  // Clear Oscillator Stop Flag for virgin DS3231
	DS3231_STATE_IDLE,          // Ready for operations, do nothing
	DS3231_STATE_RUNNING        // Active transmission in progress
}ds3231_state_t;

typedef struct
{
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t day_of_week;
	uint8_t day_of_month;
	uint8_t month;
	uint8_t year;
}time_data_t;

typedef struct
{
	// State machine
	ds3231_state_t state;

	uint32_t tick_timer;
	uint32_t last_tick;  // Last HAL_GetTick() value for calculating elapsed time
	bool time_update_request;
	bool time_update_waiting_for_interrupt;
	bool alarm1_update_request;
	bool alarm1_update_waiting_for_interrupt;
	bool alarm2_update_request;
	bool alarm2_update_waiting_for_interrupt;
	bool alarm1_read_request;
	bool alarm1_read_in_progress;
	bool alarm2_read_request;
	bool alarm2_read_in_progress;
	bool time_read_in_progress;
	bool time_read_paused;
	bool status_read_in_progress;
	bool osf_clear_pending;  // True when OSF flag needs to be cleared
	uint8_t mem_address;  // Current memory address for I2C operations
	int (* i2c_read)(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
	int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size);

	// Time and Date registers (0x00-0x06)
	time_data_t current_time;

	// Alarm 1 registers (0x07-0x0A)
	uint8_t alarm1_seconds;
	uint8_t alarm1_minutes;
	uint8_t alarm1_hours;
	uint8_t alarm1_day_date;

	// Alarm 2 registers (0x0B-0x0D)
	uint8_t alarm2_minutes;
	uint8_t alarm2_hours;
	uint8_t alarm2_day_date;

	// Control register (0x0E)
	uint8_t control;

	// Control/Status register (0x0F)
	uint8_t control_status;

	// Aging offset register (0x10)
	uint8_t aging_offset;

	// Temperature registers (0x11-0x12)
	int16_t temp;

	uint8_t tx_buffer[18];
	uint8_t rx_buffer[18];
}ds3231_data_t;
/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void ds3231_init(ds3231_data_t * this,
		int (* i2c_read)(uint8_t mem_addr, uint8_t *data, uint16_t data_size),
		int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size));
void ds3231_main(ds3231_data_t * this);
void ds3231_interrupt(ds3231_data_t * this);
void ds3231_set_time(ds3231_data_t * this, time_data_t * new_time);
void ds3231_adjust_time(ds3231_data_t * this, ds3231_time_param_t param, ds3231_adjust_dir_t dir);
void ds3231_set_alarm1(ds3231_data_t * this, uint8_t seconds, uint8_t minutes, uint8_t hours, uint8_t day_date);
void ds3231_set_alarm2(ds3231_data_t * this, uint8_t minutes, uint8_t hours, uint8_t day_date);
void ds3231_read_alarm1(ds3231_data_t * this);
void ds3231_read_alarm2(ds3231_data_t * this);
time_data_t * ds3231_get_time(ds3231_data_t * this);
void ds3231_pause_time_read(ds3231_data_t * this);
void ds3231_resume_time_read(ds3231_data_t * this);
void ds3231_error(ds3231_data_t * this);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_DS3231_DS3231_H_ */
