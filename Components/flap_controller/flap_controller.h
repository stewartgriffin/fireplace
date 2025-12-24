/***********************************************************************************************************************
 *
 *            File: flap_controller.h
 *      Created on: Dec 24, 2024
 *          Author: Claude Code
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

#ifndef COMPONENTS_FLAP_CONTROLLER_FLAP_CONTROLLER_H_
#define COMPONENTS_FLAP_CONTROLLER_FLAP_CONTROLLER_H_

/**************************************           INCLUDE FILES              ******************************************/
#include <stdint.h>
#include <stdbool.h>

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief Function pointer type for PWM control
 * @param pwm_value PWM compare value (900-1600 for servo motion: 0.9ms-1.6ms)
 */
typedef void (*flap_set_pwm_t)(uint16_t pwm_value);

/**
 * @brief Flap controller data structure
 * Each flap instance is independent with its own data structure
 */
typedef struct {
	// Function pointer for hardware abstraction
	flap_set_pwm_t set_pwm;

	// Current position in fixed-point (0-10000 representing 0.00%-100.00%)
	// Scale factor: 100 (centipercent representation)
	int16_t current_position;

	// Target position (0-100%)
	uint8_t target_position;

	// Transition time in seconds (time to move from 0% to 100%)
	uint16_t transition_time_seconds;

	// Timing management
	uint32_t last_update_tick;
} flap_controller_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize flap controller
 * @param data Pointer to flap controller data structure
 * @param set_pwm Function pointer to set flap PWM
 * @param transition_time_seconds Time in seconds for full 0-100% transition (0 = instantaneous, recommended: 5-10 seconds)
 */
void flap_controller_init(flap_controller_data_t *data,
						  flap_set_pwm_t set_pwm,
						  uint16_t transition_time_seconds);

/**
 * @brief Main function - call periodically to update flap position
 * @param data Pointer to flap controller data structure
 */
void flap_controller_main(flap_controller_data_t *data);

/**
 * @brief Set target position for flap
 * @param data Pointer to flap controller data structure
 * @param position Target position in percentage (0-100)
 */
void flap_controller_set_position(flap_controller_data_t *data, uint8_t position);

/**
 * @brief Get current flap position
 * @param data Pointer to flap controller data structure
 * @return Current position in percentage (0-100)
 */
uint8_t flap_controller_get_position(flap_controller_data_t *data);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_FLAP_CONTROLLER_FLAP_CONTROLLER_H_ */
