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

/**
 * @brief Direction of temperature change for hysteresis filtering
 */
typedef enum
{
	MAX6675_DIRECTION_NONE,     ///< No direction established yet (initial state)
	MAX6675_DIRECTION_RISING,   ///< Temperature trending upward
	MAX6675_DIRECTION_FALLING,  ///< Temperature trending downward
} max6675_direction_t;

/**
 * @brief MAX6675 thermocouple-to-digital converter driver data structure
 * Reads temperature from K-type thermocouple via SPI interface
 * Temperature resolution: 0.25°C (12-bit)
 * Update rate: Approximately every 220ms
 */
typedef struct
{
	/**
	 * @brief Function pointer to start SPI transfer
	 * @param tx_buffer Pointer to transmit buffer
	 * @param rx_buffer Pointer to receive buffer
	 * @param size Number of bytes to transfer
	 * @return 0 on success, error code otherwise
	 */
	int (*spi_start_transfer)(uint8_t *tx_buffer, uint8_t * rx_buffer, uint16_t size);

	uint32_t temperature;           ///< Last raw temperature reading in degrees Celsius
	uint32_t filtered_temperature;  ///< Filtered temperature with directional hysteresis
	max6675_direction_t direction;  ///< Current accepted direction of temperature change
	uint32_t tick_timer;            ///< Tick counter for timing
	uint32_t last_tick;             ///< Last HAL_GetTick() value for calculating elapsed time
	bool conection_open;            ///< True if thermocouple is connected
	bool transfer_in_progress;      ///< True if SPI transfer is active
	uint8_t tx_buffer[2];           ///< SPI transmit buffer
	uint8_t rx_buffer[2];           ///< SPI receive buffer
}max6675_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize MAX6675 thermocouple driver
 * @param this Pointer to MAX6675 data structure
 * @param spi_start_transfer Function pointer to start SPI transfer (interrupt-driven)
 */
void max6675_init(max6675_data_t * this,
				int (*spi_start_transfer)(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size));

/**
 * @brief Main function - call periodically to trigger temperature readings
 * Automatically requests new readings at appropriate intervals
 * @param this Pointer to MAX6675 data structure
 */
void max6675_main(max6675_data_t * this);

/**
 * @brief Get last measured temperature
 * @param this Pointer to MAX6675 data structure
 * @return Temperature in degrees Celsius
 */
int max6675_get_temperature(max6675_data_t * this);

/**
 * @brief SPI transfer complete interrupt callback
 * Call from SPI interrupt handler when transfer completes
 * @param this Pointer to MAX6675 data structure
 */
void max6675_spi_irq_handler(max6675_data_t * this);


/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_MAX6675_MAX6675_H_ */
