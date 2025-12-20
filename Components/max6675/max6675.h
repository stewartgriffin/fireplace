/***********************************************************************************************************************
 *
 *            File: max6675.h
 *      Created on: Oct 16, 2025 10:53:52 PM
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef SRC_DRIVERS_MAX6675_MAX6675_H_
#define SRC_DRIVERS_MAX6675_MAX6675_H_

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
	int (*spi_start_transfer)(uint8_t *tx_buffer, uint8_t * rx_buffer, uint16_t size);
	uint32_t temperature;
	bool conection_open;
	uint8_t tx_buffer[2];
	uint8_t rx_buffer[2];
}max6675_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void max6675_init(max6675_data_t * this,
				int (*spi_start_transfer)(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size));
void max6675_main(max6675_data_t * this);
int max6675_get_temperature(max6675_data_t * this);
void max6675_spi_irq_handler(max6675_data_t * this);


/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_MAX6675_MAX6675_H_ */
