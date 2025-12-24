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

/**
 * @brief HD44780 driver state machine states
 */
typedef enum
{
	HD44780_STATE_UNINIT,       ///< Uninitialized state
	HD44780_STATE_INITIALIZING, ///< Performing initialization sequence
	HD44780_STATE_IDLE,         ///< Idle, ready for commands
	HD44780_STATE_TRANSFER      ///< Transferring data to display
}hd44780_state_t;

/**
 * @brief HD44780 byte transfer state machine
 * Handles 4-bit mode transfers (each byte sent as two nibbles)
 */
typedef enum
{
	HD44780_TRANSFER_IDLE,                  ///< No transfer in progress
	HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH,    ///< High nibble, E=1 (latch)
	HD44780_TRANSFER_HIGH_NIBBLE_E_LOW,     ///< High nibble, E=0 (complete)
	HD44780_TRANSFER_LOW_NIBBLE_E_HIGH,     ///< Low nibble, E=1 (latch)
	HD44780_TRANSFER_LOW_NIBBLE_E_LOW,      ///< Low nibble, E=0 (complete)
	HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH,  ///< Single nibble (init), E=1
	HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW    ///< Single nibble (init), E=0
}hd44780_transfer_state_t;

/**
 * @brief HD44780 LCD driver data structure
 * Manages state machine for HD44780-compatible displays in 4-bit mode
 * Typically connected via I2C GPIO expander (e.g., PCF8574)
 */
typedef struct
{
	// Timing management
	uint32_t tick_timer;                ///< Tick counter for timing
	uint32_t last_tick;                 ///< Last HAL_GetTick() value for calculating elapsed time
	uint32_t delay;                     ///< Delay countdown in ms before next command

	// Initialization
	uint8_t init_step;                  ///< Current initialization step
	bool transfer_complete;             ///< Set by interrupt when I2C transfer completes

	// State machines
	hd44780_state_t state;              ///< Main driver state
	hd44780_transfer_state_t transfer_state; ///< Byte transfer state

	// Current transfer data
	uint8_t current_byte;               ///< Current byte being transferred
	uint8_t single_nibble_value;        ///< Nibble value during single-nibble init commands
	bool current_rs;                    ///< Current register select (0=command, 1=data)

	// Display configuration
	uint8_t rows;                       ///< Number of rows (e.g., 2 or 4)
	uint8_t columns;                    ///< Number of columns (e.g., 16 or 20)

	// Buffer write state
	bool buffer_write_in_progress;      ///< Buffer write operation active
	uint16_t buffer_write_index;        ///< Current position in buffer
	uint16_t buffer_write_total;        ///< Total buffer size
	const char * buffer_ptr;            ///< Pointer to buffer being written

	/**
	 * @brief Function pointer for updating display pins via GPIO expander
	 * @param d4_d7 4-bit data nibble (lower 4 bits)
	 * @param rs Register select (0=command, 1=data)
	 * @param e Enable signal (pulse this to latch data)
	 */
	void (* update_pins)(uint8_t d4_d7, bool rs, bool e);

}hd44780_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize HD44780 LCD display driver
 * @param this Pointer to HD44780 data structure
 * @param rows Number of display rows (2 or 4)
 * @param columns Number of display columns (16 or 20)
 * @param update_pins Function pointer to update display pins (via GPIO expander)
 */
void hd44780_init(hd44780_data_t * this,
		uint8_t rows,
		uint8_t columns,
		void (* update_pins)(uint8_t d4_d7, bool rs, bool e));

/**
 * @brief Main function - call periodically to update display state machine
 * Handles initialization sequence, timing delays, and character transfers
 * @param this Pointer to HD44780 data structure
 */
void hd44780_main(hd44780_data_t * this);

/**
 * @brief I2C transfer complete interrupt callback
 * Call from I2C interrupt handler when GPIO expander write completes
 * @param this Pointer to HD44780 data structure
 */
void hd44780_transfer_complete(hd44780_data_t * this);

/**
 * @brief Write buffer to display
 * Writes characters sequentially to display (rows * columns bytes)
 * @param this Pointer to HD44780 data structure
 * @param buffer Pointer to buffer containing display data
 * @return 0 on success, -1 if write already in progress
 */
int hd44780_write_buffer(hd44780_data_t * this, const char * buffer);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_HD44780_HD44780_H_ */
