/***********************************************************************************************************************
 *
 *            File: hd44780.h
 *      Created on: Dec 21, 2025
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef SRC_DRIVERS_HD44780_HD44780_H_
#define SRC_DRIVERS_HD44780_HD44780_H_

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
	HD44780_STATE_IDLE,
	HD44780_STATE_SEND_HIGH_NIBBLE,
	HD44780_STATE_SEND_LOW_NIBBLE,
	HD44780_STATE_WAIT_TRANSFER
}hd44780_state_t;

typedef struct
{
	uint32_t tick_timer;
	bool init_in_progress;
	uint8_t init_step;

	hd44780_state_t state;
	uint8_t current_byte;
	bool current_rs;
	bool high_nibble_sent;

	// Display configuration
	uint8_t rows;
	uint8_t columns;

	// Buffer write state
	bool buffer_write_in_progress;
	uint16_t buffer_write_index;
	uint16_t buffer_write_total;
	const char * buffer_ptr;

	// Function pointer for updating all display pins
	// d4_d7: 4-bit data nibble (lower 4 bits)
	// rs: Register select (0=command, 1=data)
	// e: Enable signal (pulse this to latch data)
	void (* update_pins)(uint8_t d4_d7, bool rs, bool e);

}hd44780_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void hd44780_init(hd44780_data_t * this,
		uint8_t rows,
		uint8_t columns,
		void (* update_pins)(uint8_t d4_d7, bool rs, bool e));
void hd44780_main(hd44780_data_t * this, uint32_t call_period_ms);
void hd44780_transfer_complete(hd44780_data_t * this);
void hd44780_write_buffer(hd44780_data_t * this, const char * buffer);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_HD44780_HD44780_H_ */
