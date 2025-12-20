/***********************************************************************************************************************
 *
 *            File: fireplace.c
 *      Created on: Oct 25, 2025 9:55:48 PM
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "max6675.h"
#include "spi.h"

#include "ds3231.h"
#include "i2c.h"

/**************************************           DEFINES                    ******************************************/

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
int main_function_timer = 0;
max6675_data_t thermocouple;
int current_temperature = 0;

ds3231_data_t clock;

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size);
int clock_i2c_send(uint8_t *data, uint16_t data_size);
int clock_i2c_receive(uint8_t *data, uint16_t data_size);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void fireplace_init(void)
{
	max6675_init(&thermocouple, thermocouple_spi_send_receive_wrapper);
	ds3231_init(&clock, clock_i2c_send, clock_i2c_receive);
}

void fireplace_main(void)
{
	if (main_function_timer % 1000 == 0)
	{
		max6675_main(&thermocouple);
		ds3231_main(&clock);
	}

	current_temperature = max6675_get_temperature(&thermocouple);
	main_function_timer++;

	if (main_function_timer == 1000)
	{
		main_function_timer = 0;
	}
}

void clock_i2c_interrupt(void)
{
	ds3231_interrupt(&clock);
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size)
{
	return HAL_SPI_TransmitReceive(&hspi2, tx_buffer, rx_buffer, size, 1000);
}

int clock_i2c_send(uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Mem_Write_IT(&hi2c2, 0x68 << 1, 0x00, 1, data, data_size);
}

int clock_i2c_receive(uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Mem_Read_IT(&hi2c2, 0x68 << 1, 0x00, 1, data, data_size);
}
