/***********************************************************************************************************************
 *
 *            File: flap_controller.c
 *      Created on: Dec 24, 2024
 *          Author: Claude Code
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "flap_controller.h"
#include "stm32h5xx_hal.h"

/**************************************           DEFINES                    ******************************************/
#define SERVO_PWM_MIN 900      // Minimum PWM value (0.9ms pulse = 0%, fully closed)
#define SERVO_PWM_MAX 1600     // Maximum PWM value (1.6ms pulse = 100%, fully open)
#define POSITION_SCALE 100     // Fixed-point scale factor (centipercent)
#define POSITION_MAX 10000     // Maximum position in fixed-point (100.00%)

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static uint16_t position_to_pwm(int16_t position);
static int16_t clamp_position(int16_t position);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/

void flap_controller_init(flap_controller_data_t *data,
						  flap_set_pwm_t set_pwm,
						  uint16_t transition_time_seconds)
{
	data->set_pwm = set_pwm;
	data->current_position = 0;
	data->target_position = 0;
	data->transition_time_seconds = transition_time_seconds;
	data->last_update_tick = HAL_GetTick();

	// Set initial PWM to 0% position
	if (data->set_pwm != NULL)
	{
		data->set_pwm(position_to_pwm(0));
	}
}

void flap_controller_main(flap_controller_data_t *data)
{
	uint32_t current_tick = HAL_GetTick();
	uint32_t elapsed_ms = current_tick - data->last_update_tick;

	// Update only if time has passed
	if (elapsed_ms == 0)
	{
		return;
	}

	data->last_update_tick = current_tick;

	// Calculate error (how far we are from target)
	// Target is 0-100, current is 0-10000, so scale target up
	int16_t target_scaled = (int16_t)data->target_position * POSITION_SCALE;
	int16_t error = target_scaled - data->current_position;

	// If transition time is 0, change instantaneously (no rate limiting)
	int16_t position_change;
	if (data->transition_time_seconds == 0)
	{
		// Instantaneous change - jump directly to target
		position_change = error;
	}
	else
	{
		// Calculate how much the position should change (fixed-point arithmetic)
		// transition_time_seconds is the time to go from 0% to 100% (0 to 10000 in fixed-point)
		// Rate: 10000 units per (transition_time_seconds * 1000) ms
		// Simplified: 10 units per transition_time_seconds ms
		// Max change = (10 * elapsed_ms) / transition_time_seconds
		// To maximize precision, multiply first: (POSITION_MAX * elapsed_ms) / (transition_time_seconds * 1000)
		int32_t max_change = ((int32_t)POSITION_MAX * (int32_t)elapsed_ms) / ((int32_t)data->transition_time_seconds * 1000);

		// Apply rate limiting (low-pass filter behavior)
		if (error > max_change)
		{
			// Target is higher, move up at maximum rate
			position_change = (int16_t)max_change;
		}
		else if (error < -max_change)
		{
			// Target is lower, move down at maximum rate
			position_change = -(int16_t)max_change;
		}
		else
		{
			// Within one step of target, just set to target
			position_change = error;
		}
	}

	// Update current position
	data->current_position += position_change;
	data->current_position = clamp_position(data->current_position);

	// Update PWM output
	if (data->set_pwm != NULL)
	{
		uint16_t pwm_value = position_to_pwm(data->current_position);
		data->set_pwm(pwm_value);
	}
}

void flap_controller_set_position(flap_controller_data_t *data, uint8_t position)
{
	// Clamp position to 0-100%
	if (position > 100)
	{
		position = 100;
	}

	data->target_position = position;
}

uint8_t flap_controller_get_position(flap_controller_data_t *data)
{
	// Convert from fixed-point (0-10000) to percentage (0-100)
	// Divide by scale factor and round to nearest integer
	uint8_t position = (uint8_t)((data->current_position + (POSITION_SCALE / 2)) / POSITION_SCALE);

	// Clamp to valid range
	if (position > 100)
	{
		position = 100;
	}

	return position;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/

/**
 * @brief Convert position to PWM value using fixed-point arithmetic
 * @param position Position in fixed-point (0-10000 representing 0.00%-100.00%)
 * @return PWM value (900 - 1600)
 */
static uint16_t position_to_pwm(int16_t position)
{
	// Clamp position to valid range
	position = clamp_position(position);

	// Linear mapping: 0 -> 900 (0.9ms, fully closed), 10000 -> 1600 (1.6ms, fully open)
	// PWM = SERVO_PWM_MIN + (position * (SERVO_PWM_MAX - SERVO_PWM_MIN)) / POSITION_MAX
	// PWM = 900 + (position * 700) / 10000
	// Multiply first to maintain precision, then divide
	uint16_t pwm = SERVO_PWM_MIN + ((uint32_t)position * (SERVO_PWM_MAX - SERVO_PWM_MIN)) / POSITION_MAX;

	return pwm;
}

/**
 * @brief Clamp position to valid range
 * @param position Position in fixed-point
 * @return Clamped position (0-10000)
 */
static int16_t clamp_position(int16_t position)
{
	if (position < 0)
	{
		return 0;
	}
	else if (position > POSITION_MAX)
	{
		return POSITION_MAX;
	}
	return position;
}
