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
 * @brief Function pointer type for GPIO control
 * @param state 1 to activate (set high), 0 to deactivate (set low)
 */
typedef void (*flap_set_gpio_t)(uint8_t state);

/**
 * @brief Internal state of flap motion
 */
typedef enum {
	FLAP_STATE_INIT,	// No motion, unknown position after power up, needs to be set to known position
    FLAP_STATE_IDLE,    // No motion, both pins inactive
    FLAP_STATE_OPENING, // Open pin active, counting elapsed time
    FLAP_STATE_CLOSING  // Close pin active, counting elapsed time
} flap_state_t;

/**
 * @brief Flap controller data structure
 * Each flap instance is independent with its own data structure.
 * current_position represents the known position (0-100%) after completed motions.
 * On startup it is assumed to be 0 (fully closed). Call set_position(0) to physically
 * enforce a known closed state before first use.
 */
typedef struct {
    // Function pointers for hardware abstraction
    flap_set_gpio_t set_open_pin;
    flap_set_gpio_t set_close_pin;

    // Current known position (0-100%), updated when motion completes
    uint8_t current_position;

    // Requested target position (0-100%)
    uint8_t target_position;

    // Time in milliseconds for a full 0->100% travel (opening)
    uint32_t open_travel_time_ms;

    // Time in milliseconds for a full 100->0% travel (closing)
    uint32_t close_travel_time_ms;

    // Internal motion state
    flap_state_t state;
    uint32_t motion_start_tick;
    uint32_t motion_duration_ms;

    // Position validity — false after a swipe; restored when an end stop is reached
    bool position_valid;

    // Recalibration: when position is unknown and a non-end-stop target is requested,
    // the flap is first driven to 100% to establish a known position, then to pending_target.
    bool    recalibrating;
    uint8_t pending_target;
} flap_controller_data_t;

/**************************************           DEFINES                    ******************************************/

/** Default travel times if not specified */
#define FLAP_DEFAULT_OPEN_TRAVEL_TIME_MS  4500U
#define FLAP_DEFAULT_CLOSE_TRAVEL_TIME_MS 6000U

/** Duration of a single swipe pulse in milliseconds */
#define FLAP_SWIPE_DURATION_MS 100U

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize flap controller
 * @param data Pointer to flap controller data structure
 * @param set_open_pin Function pointer to activate/deactivate the open GPIO pin
 * @param set_close_pin Function pointer to activate/deactivate the close GPIO pin
 * @param open_travel_time_ms Time in milliseconds for a full 0->100% travel
 * @param close_travel_time_ms Time in milliseconds for a full 100->0% travel
 */
void flap_controller_init(flap_controller_data_t *data,
                          flap_set_gpio_t set_open_pin,
                          flap_set_gpio_t set_close_pin,
                          uint32_t open_travel_time_ms,
                          uint32_t close_travel_time_ms);

/**
 * @brief Main function - call periodically (e.g. every 10ms) to drive motion timing
 * @param data Pointer to flap controller data structure
 */
void flap_controller_main(flap_controller_data_t *data);

/**
 * @brief Request a new target position
 * If motion is already in progress it is stopped and current_position is
 * recalculated from elapsed time before the new motion begins.
 * @param data Pointer to flap controller data structure
 * @param position Target position in percentage (0-100)
 */
void flap_controller_set_position(flap_controller_data_t *data, uint8_t position);

/**
 * @brief Get the last confirmed position (updated when motion completes)
 * @param data Pointer to flap controller data structure
 * @return Position in percentage (0-100)
 */
uint8_t flap_controller_get_position(flap_controller_data_t *data);

/**
 * @brief Pulse the flap in the open direction for FLAP_SWIPE_DURATION_MS (100 ms).
 * Marks position as unknown; the next set_position call with a non-end-stop target
 * will recalibrate by driving to 100% first.
 * @param data Pointer to flap controller data structure
 */
void flap_controller_swipe_open(flap_controller_data_t *data);

/**
 * @brief Pulse the flap in the close direction for FLAP_SWIPE_DURATION_MS (100 ms).
 * Marks position as unknown; the next set_position call with a non-end-stop target
 * will recalibrate by driving to 100% first.
 * @param data Pointer to flap controller data structure
 */
void flap_controller_swipe_close(flap_controller_data_t *data);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_FLAP_CONTROLLER_FLAP_CONTROLLER_H_ */
