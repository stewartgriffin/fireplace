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
	bool time_update_request;
	bool time_update_waiting_for_interrupt;
	int (* i2c_read)(uint8_t *data, uint16_t data_size);
	int (* i2c_write)(uint8_t *data, uint16_t data_size);
	time_data_t current_time;
	uint8_t control;
	uint8_t control_status;
	uint8_t aging_offset;
	int16_t temp;
	uint8_t tx_buffer[18];
	uint8_t rx_buffer[18];
}ds3231_data_t;
/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void ds3231_init(ds3231_data_t * this,
		int (* i2c_read)(uint8_t *data, uint16_t data_size),
		int (* i2c_write)(uint8_t *data, uint16_t data_size));
void ds3231_main(ds3231_data_t * this);
void ds3231_interrupt(ds3231_data_t * this);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_DS3231_DS3231_H_ */
