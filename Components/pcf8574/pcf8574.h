/***********************************************************************************************************************
 *
 *            File: pcf8574.h
 *      Created on: Dec 21, 2025
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef SRC_DRIVERS_PCF8574_PCF8574_H_
#define SRC_DRIVERS_PCF8574_PCF8574_H_

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief PCF8574 I2C GPIO expander driver data structure
 * 8-bit I/O expander with interrupt capability
 * Typically used to interface HD44780 LCD displays via I2C
 */
typedef struct
{
	uint32_t tick_timer;                ///< Tick counter for timing
	bool write_request;                 ///< Write request pending
	bool write_in_progress;             ///< Write operation in progress

	uint8_t output_state;               ///< Current output state of all 8 pins

	/**
	 * @brief Function pointer to start I2C write transfer
	 * @param mem_addr Memory address (unused for PCF8574)
	 * @param data Pointer to data buffer
	 * @param data_size Number of bytes to write
	 * @return 0 on success, error code otherwise
	 */
	int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size);

	/**
	 * @brief Callback function called when I2C write completes
	 * Use this to notify other modules (e.g., HD44780) that pin update is complete
	 */
	void (* write_complete_callback)(void);

	uint8_t tx_buffer[1];               ///< I2C transmit buffer
}pcf8574_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize PCF8574 GPIO expander driver
 * @param this Pointer to PCF8574 data structure
 * @param i2c_write Function pointer to start I2C write (interrupt-driven)
 * @param write_complete_callback Function pointer called when write completes (can be NULL)
 */
void pcf8574_init(pcf8574_data_t * this,
		int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size),
		void (* write_complete_callback)(void));

/**
 * @brief Main function - call periodically to process write requests
 * Handles queued write operations to GPIO expander
 * @param this Pointer to PCF8574 data structure
 */
void pcf8574_main(pcf8574_data_t * this);

/**
 * @brief I2C transfer complete interrupt callback
 * Call from I2C interrupt handler when write completes
 * @param this Pointer to PCF8574 data structure
 */
void pcf8574_interrupt(pcf8574_data_t * this);

/**
 * @brief Write data to GPIO expander
 * Sets all 8 pins according to data byte (bit 0 = P0, bit 7 = P7)
 * @param this Pointer to PCF8574 data structure
 * @param data 8-bit value to write to GPIO pins
 */
void pcf8574_write(pcf8574_data_t * this, uint8_t data);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_PCF8574_PCF8574_H_ */
