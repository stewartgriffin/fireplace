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
typedef struct
{
	uint32_t tick_timer;
	bool write_request;
	bool write_in_progress;
	bool read_request;
	bool read_in_progress;

	uint8_t output_state;  // Current output state of all 8 pins
	uint8_t input_state;   // Last read input state of all 8 pins

	int (* i2c_read)(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
	int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size);

	uint8_t tx_buffer[1];
	uint8_t rx_buffer[1];
}pcf8574_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void pcf8574_init(pcf8574_data_t * this,
		int (* i2c_read)(uint8_t mem_addr, uint8_t *data, uint16_t data_size),
		int (* i2c_write)(uint8_t mem_addr, uint8_t *data, uint16_t data_size));
void pcf8574_main(pcf8574_data_t * this, uint32_t call_period_ms);
void pcf8574_interrupt(pcf8574_data_t * this);
void pcf8574_write(pcf8574_data_t * this, uint8_t data);
void pcf8574_read(pcf8574_data_t * this);
void pcf8574_set_pin(pcf8574_data_t * this, uint8_t pin, bool state);
bool pcf8574_get_pin(pcf8574_data_t * this, uint8_t pin);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_PCF8574_PCF8574_H_ */
