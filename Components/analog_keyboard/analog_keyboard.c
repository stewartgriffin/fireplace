/***********************************************************************************************************************
 *
 *            File: analog_keyboard.c
 *      Created on: Dec 23, 2025
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "analog_keyboard.h"
#include "stm32h5xx_hal.h"

/**************************************           DEFINES                    ******************************************/

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void analog_keyboard_init(analog_keyboard_data_t * this,
						int (*adc_start_conversion)(void),
						int (*adc_read)(uint32_t *value),
						void (*button_left_callback)(bool state),
						void (*button_up_callback)(bool state),
						void (*button_down_callback)(bool state),
						void (*button_right_callback)(bool state),
						void (*button_ok_callback)(bool state))
{
	this->adc_start_conversion = adc_start_conversion;
	this->adc_read = adc_read;
	this->button_left_callback = button_left_callback;
	this->button_up_callback = button_up_callback;
	this->button_down_callback = button_down_callback;
	this->button_right_callback = button_right_callback;
	this->button_ok_callback = button_ok_callback;
	this->last_tick = HAL_GetTick();
	this->adc_value = 0;
	this->conversion_in_progress = false;
	this->button_left_state = false;
	this->button_up_state = false;
	this->button_down_state = false;
	this->button_right_state = false;
	this->button_ok_state = false;
	this->button_left_prev_state = false;
	this->button_up_prev_state = false;
	this->button_down_prev_state = false;
	this->button_right_prev_state = false;
	this->button_ok_prev_state = false;
	this->button_left_flag = false;
	this->button_up_flag = false;
	this->button_down_flag = false;
	this->button_right_flag = false;
	this->button_ok_flag = false;
}

void analog_keyboard_main(analog_keyboard_data_t * this)
{
	// Check flags and call callbacks with state changes
	if (this->button_left_flag)
	{
		this->button_left_flag = false;
		if (this->button_left_callback != NULL)
		{
			this->button_left_callback(this->button_left_state);
		}
	}

	if (this->button_up_flag)
	{
		this->button_up_flag = false;
		if (this->button_up_callback != NULL)
		{
			this->button_up_callback(this->button_up_state);
		}
	}

	if (this->button_down_flag)
	{
		this->button_down_flag = false;
		if (this->button_down_callback != NULL)
		{
			this->button_down_callback(this->button_down_state);
		}
	}

	if (this->button_right_flag)
	{
		this->button_right_flag = false;
		if (this->button_right_callback != NULL)
		{
			this->button_right_callback(this->button_right_state);
		}
	}

	if (this->button_ok_flag)
	{
		this->button_ok_flag = false;
		if (this->button_ok_callback != NULL)
		{
			this->button_ok_callback(this->button_ok_state);
		}
	}

	// Start conversion if not already in progress
	if (!this->conversion_in_progress)
	{
		// Calculate elapsed time
		uint32_t current_tick = HAL_GetTick();
		uint32_t elapsed = current_tick - this->last_tick;

		// Check if it's time to start a new conversion
		if (elapsed >= ANALOG_KEYBOARD_SAMPLE_PERIOD_MS)
		{
			this->last_tick = current_tick;
			this->conversion_in_progress = true;
			this->adc_start_conversion();
		}
	}
}

void analog_keyboard_interrupt(analog_keyboard_data_t * this)
{
	// Read ADC value
	this->adc_read(&this->adc_value);
	this->conversion_in_progress = false;

	// Store previous states
	this->button_left_prev_state = this->button_left_state;
	this->button_up_prev_state = this->button_up_state;
	this->button_down_prev_state = this->button_down_state;
	this->button_right_prev_state = this->button_right_state;
	this->button_ok_prev_state = this->button_ok_state;

	// Reset all current states
	this->button_left_state = false;
	this->button_up_state = false;
	this->button_down_state = false;
	this->button_right_state = false;
	this->button_ok_state = false;

	// Determine current button state based on ADC value
	if (this->adc_value >= ANALOG_KEYBOARD_LEFT_MIN &&
		this->adc_value <= ANALOG_KEYBOARD_LEFT_MAX)
	{
		this->button_left_state = true;
	}
	else if (this->adc_value >= ANALOG_KEYBOARD_UP_MIN &&
			 this->adc_value <= ANALOG_KEYBOARD_UP_MAX)
	{
		this->button_up_state = true;
	}
	else if (this->adc_value >= ANALOG_KEYBOARD_DOWN_MIN &&
			 this->adc_value <= ANALOG_KEYBOARD_DOWN_MAX)
	{
		this->button_down_state = true;
	}
	else if (this->adc_value >= ANALOG_KEYBOARD_RIGHT_MIN &&
			 this->adc_value <= ANALOG_KEYBOARD_RIGHT_MAX)
	{
		this->button_right_state = true;
	}
	else if (this->adc_value >= ANALOG_KEYBOARD_OK_MIN &&
			 this->adc_value <= ANALOG_KEYBOARD_OK_MAX)
	{
		this->button_ok_state = true;
	}

	// Set flags only if state changed
	if (this->button_left_state != this->button_left_prev_state)
	{
		this->button_left_flag = true;
	}
	if (this->button_up_state != this->button_up_prev_state)
	{
		this->button_up_flag = true;
	}
	if (this->button_down_state != this->button_down_prev_state)
	{
		this->button_down_flag = true;
	}
	if (this->button_right_state != this->button_right_prev_state)
	{
		this->button_right_flag = true;
	}
	if (this->button_ok_state != this->button_ok_prev_state)
	{
		this->button_ok_flag = true;
	}
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
