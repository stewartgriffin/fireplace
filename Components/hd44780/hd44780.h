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
	HD44780_STATE_UNINIT,
	HD44780_STATE_INITIALIZING,
	HD44780_STATE_IDLE,
	HD44780_STATE_TRANSFER
}hd44780_state_t;

typedef enum
{
	HD44780_TRANSFER_IDLE,
	HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH,
	HD44780_TRANSFER_HIGH_NIBBLE_E_LOW,
	HD44780_TRANSFER_LOW_NIBBLE_E_HIGH,
	HD44780_TRANSFER_LOW_NIBBLE_E_LOW,
	HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH,
	HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW
}hd44780_transfer_state_t;

typedef struct
{
	uint32_t tick_timer;
	uint32_t last_tick;  // Last HAL_GetTick() value for calculating elapsed time
	uint32_t delay;      // Delay countdown in ms before next command
	uint8_t init_step;
	bool transfer_complete;  // Set by interrupt when I2C transfer completes

	hd44780_state_t state;
	hd44780_transfer_state_t transfer_state;
	uint8_t current_byte;
	uint8_t single_nibble_value;  // For storing nibble value during single-nibble init commands
	bool current_rs;

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
void hd44780_main(hd44780_data_t * this);
void hd44780_transfer_complete(hd44780_data_t * this);
int hd44780_write_buffer(hd44780_data_t * this, const char * buffer);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_HD44780_HD44780_H_ */
