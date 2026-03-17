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

// When targeting an end stop (0% or 100%), run for this fixed duration regardless of
// calculated travel time, to prevent positional error accumulation over time.
#define TRAVEL_TIME_MAX_MS 6200U

// Duration of the closing pulse applied at startup to establish a known 0% position.
#define INIT_CLOSE_DURATION_MS 7000U

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static void stop_motion(flap_controller_data_t *data);
static uint8_t calc_position_change_percent(uint32_t travel_time_ms, uint32_t elapsed_ms);
static void start_swipe(flap_controller_data_t *data, flap_state_t direction);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/

void flap_controller_init(flap_controller_data_t *data,
                          flap_set_gpio_t set_open_pin,
                          flap_set_gpio_t set_close_pin,
                          uint32_t open_travel_time_ms,
                          uint32_t close_travel_time_ms)
{
    data->set_open_pin = set_open_pin;
    data->set_close_pin = set_close_pin;
    data->open_travel_time_ms  = (open_travel_time_ms  > 0) ? open_travel_time_ms  : FLAP_DEFAULT_OPEN_TRAVEL_TIME_MS;
    data->close_travel_time_ms = (close_travel_time_ms > 0) ? close_travel_time_ms : FLAP_DEFAULT_CLOSE_TRAVEL_TIME_MS;
    data->current_position = 0;
    data->target_position = 0;
    data->state = FLAP_STATE_INIT;
    data->motion_start_tick = HAL_GetTick();
    data->motion_duration_ms = INIT_CLOSE_DURATION_MS;
    data->position_valid = false;
    data->recalibrating = false;
    data->pending_target = 0;

    // Drive close pin for init homing, ensure open pin is inactive
    if (data->set_open_pin != NULL)
    {
        data->set_open_pin(0);
    }
    if (data->set_close_pin != NULL)
    {
        data->set_close_pin(1);
    }
}

void flap_controller_main(flap_controller_data_t *data)
{
    if (data->state == FLAP_STATE_INIT)
    {
        uint32_t elapsed_ms = HAL_GetTick() - data->motion_start_tick;
        if (elapsed_ms >= data->motion_duration_ms)
        {
            stop_motion(data);
            data->current_position = 0;
            data->target_position = 0;
            data->position_valid = true;
            data->state = FLAP_STATE_IDLE;
        }
        return;
    }

    if (data->state == FLAP_STATE_IDLE)
    {
        if (data->target_position == data->current_position || data->target_position > 100U)
        {
            return;
        }

        // Ensure both pins are off before asserting either direction
        stop_motion(data);

        // Start new motion toward target
        uint8_t diff;
        if (data->target_position > data->current_position)
        {
            diff = data->target_position - data->current_position;
            data->state = FLAP_STATE_OPENING;
            if (data->set_open_pin != NULL)
            {
                data->set_open_pin(1);
            }
        }
        else
        {
            diff = data->current_position - data->target_position;
            data->state = FLAP_STATE_CLOSING;
            if (data->set_close_pin != NULL)
            {
                data->set_close_pin(1);
            }
        }

        // When targeting an end stop use a fixed max duration to absorb accumulated error.
        // Otherwise calculate proportionally from the direction-specific travel time.
        uint32_t travel_time_ms = (data->state == FLAP_STATE_OPENING) ? data->open_travel_time_ms : data->close_travel_time_ms;
        if (data->target_position == 0U || data->target_position == 100U)
        {
            data->motion_duration_ms = TRAVEL_TIME_MAX_MS;
        }
        else
        {
            data->motion_duration_ms = ((uint32_t)diff * travel_time_ms) / 100U;
        }
        data->motion_start_tick = HAL_GetTick();
    }
    else
    {
        uint32_t elapsed_ms = HAL_GetTick() - data->motion_start_tick;

        if (elapsed_ms >= data->motion_duration_ms)
        {
            stop_motion(data);

            if (data->recalibrating)
            {
                // Reached 100% — position is now known; start motion to the real target
                data->current_position = 100U;
                data->position_valid   = true;
                data->recalibrating    = false;
                data->target_position  = data->pending_target;
                data->state            = FLAP_STATE_IDLE;
            }
            else if (data->target_position > 100U)
            {
                // Swipe completed — position is still unknown
                data->state = FLAP_STATE_IDLE;
            }
            else
            {
                // Normal motion complete
                data->current_position = data->target_position;
                data->state            = FLAP_STATE_IDLE;
                // End-stop motions restore position validity
                if (data->target_position == 0U || data->target_position == 100U)
                {
                    data->position_valid = true;
                }
            }
        }
    }
}

void flap_controller_set_position(flap_controller_data_t *data, uint8_t position)
{
    if (data->state == FLAP_STATE_INIT)
    {
        return;
    }

    if (position > 100U)
    {
        position = 100U;
    }

    // If already recalibrating to 100%, update the pending target.
    // End-stop targets (0, 100) abort the recalibration and proceed directly
    // since the physical end stop will establish a known position.
    if (data->recalibrating)
    {
        if (position == 0U || position == 100U)
        {
            stop_motion(data);
            data->recalibrating    = false;
            data->current_position = 0U;  // worst case for full-travel timing
            data->state            = FLAP_STATE_IDLE;
            // Fall through to proceed as a normal end-stop move
        }
        else
        {
            data->pending_target = position;
            return;
        }
    }

    // If position is unknown (after swipe) and target is not an end stop,
    // drive to 100% first to establish a known position, then to target.
    if (!data->position_valid && position != 0U && position != 100U)
    {
        if (data->state != FLAP_STATE_IDLE)
        {
            stop_motion(data);
            data->state = FLAP_STATE_IDLE;
        }
        data->current_position = 0U;   // assume worst case for full-travel timing
        data->pending_target   = position;
        data->recalibrating    = true;
        data->target_position  = 100U;
        return;
    }

    if (position == data->target_position)
    {
        return;
    }

    // If motion is in progress, stop and recalculate current position from elapsed time
    if (data->state != FLAP_STATE_IDLE)
    {
        stop_motion(data);

        if (data->target_position <= 100U && data->position_valid)
        {
            // Normal motion interrupted — estimate position from elapsed time
            uint32_t elapsed_ms = HAL_GetTick() - data->motion_start_tick;
            if (elapsed_ms > data->motion_duration_ms)
            {
                elapsed_ms = data->motion_duration_ms;
            }
            flap_state_t prev_state = data->state;
            uint32_t travel_time_ms = (prev_state == FLAP_STATE_OPENING) ? data->open_travel_time_ms : data->close_travel_time_ms;
            uint8_t moved = calc_position_change_percent(travel_time_ms, elapsed_ms);

            if (prev_state == FLAP_STATE_OPENING)
            {
                data->current_position += moved;
                if (data->current_position > 100U)
                {
                    data->current_position = 100U;
                }
            }
            else
            {
                data->current_position = (data->current_position >= moved) ? data->current_position - moved : 0U;
            }
        }
        // If position was unknown (swipe interrupted), leave current_position as-is;
        // the end-stop timing (TRAVEL_TIME_MAX_MS) will handle end-stop targets.

        data->state = FLAP_STATE_IDLE;
    }

    data->target_position = position;
}

uint8_t flap_controller_get_position(flap_controller_data_t *data)
{
    return data->current_position;
}

void flap_controller_swipe_open(flap_controller_data_t *data)
{
    start_swipe(data, FLAP_STATE_OPENING);
}

void flap_controller_swipe_close(flap_controller_data_t *data)
{
    start_swipe(data, FLAP_STATE_CLOSING);
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/

/**
 * @brief Deactivate both GPIO pins without updating state
 * @param data Pointer to flap controller data structure
 */
static void stop_motion(flap_controller_data_t *data)
{
    if (data->set_open_pin != NULL)
    {
        data->set_open_pin(0);
    }
    if (data->set_close_pin != NULL)
    {
        data->set_close_pin(0);
    }
}

/**
 * @brief Calculate how many percentage points the flap has moved in the given elapsed time
 * @param data Pointer to flap controller data structure
 * @param elapsed_ms Elapsed time in milliseconds (must be <= motion_duration_ms)
 * @return Position change in percentage points (0-100)
 */
static uint8_t calc_position_change_percent(uint32_t travel_time_ms, uint32_t elapsed_ms)
{
    if (travel_time_ms == 0U)
    {
        return 100U;
    }

    // position_change [%] = elapsed_ms * 100 / travel_time_ms
    uint32_t moved = (elapsed_ms * 100U) / travel_time_ms;
    if (moved > 100U)
    {
        moved = 100U;
    }
    return (uint8_t)moved;
}

/**
 * @brief Common implementation for swipe_open / swipe_close.
 * Stops any ongoing motion, pulses the specified direction for FLAP_SWIPE_DURATION_MS,
 * and marks position as unknown.
 * @param data      Pointer to flap controller data structure
 * @param direction FLAP_STATE_OPENING or FLAP_STATE_CLOSING
 */
static void start_swipe(flap_controller_data_t *data, flap_state_t direction)
{
    if (data->state == FLAP_STATE_INIT)
    {
        return;
    }

    // Stop any motion currently in progress
    if (data->state != FLAP_STATE_IDLE)
    {
        stop_motion(data);
        data->state = FLAP_STATE_IDLE;
    }

    // Cancel any pending recalibration — the swipe takes precedence
    data->recalibrating = false;

    // Estimate position change from the swipe pulse for display purposes.
    // Absolute accuracy is not guaranteed (hence position_valid = false), but
    // this keeps the displayed value moving in the right direction.
    {
        uint32_t travel_time_ms = (direction == FLAP_STATE_OPENING) ? data->open_travel_time_ms : data->close_travel_time_ms;
        uint8_t delta = calc_position_change_percent(travel_time_ms, FLAP_SWIPE_DURATION_MS);
        if (direction == FLAP_STATE_OPENING)
        {
            data->current_position = (data->current_position + delta <= 100U) ? data->current_position + delta : 100U;
        }
        else
        {
            data->current_position = (data->current_position >= delta) ? data->current_position - delta : 0U;
        }
    }

    // Mark position as unknown (estimate above is approximate)
    data->position_valid  = false;
    // Use >100 sentinel so the motion-complete handler knows this was a swipe
    data->target_position = 0xFFU;

    stop_motion(data);  // ensure both pins off before asserting direction

    if (direction == FLAP_STATE_OPENING)
    {
        if (data->set_open_pin != NULL) { data->set_open_pin(1); }
    }
    else
    {
        if (data->set_close_pin != NULL) { data->set_close_pin(1); }
    }

    data->state              = direction;
    data->motion_start_tick  = HAL_GetTick();
    data->motion_duration_ms = FLAP_SWIPE_DURATION_MS;
}
