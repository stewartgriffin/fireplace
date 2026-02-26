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
#define TRAVEL_TIME_MAX_MS 5000U

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static void stop_motion(flap_controller_data_t *data);
static uint8_t calc_position_change_percent(uint32_t travel_time_ms, uint32_t elapsed_ms);

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
    data->state = FLAP_STATE_IDLE;
    data->motion_start_tick = 0;
    data->motion_duration_ms = 0;

    // Ensure both pins are inactive
    if (data->set_open_pin != NULL)
    {
        data->set_open_pin(0);
    }
    if (data->set_close_pin != NULL)
    {
        data->set_close_pin(0);
    }
}

void flap_controller_main(flap_controller_data_t *data)
{
    if (data->state == FLAP_STATE_IDLE)
    {
        if (data->target_position == data->current_position)
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
            // Motion complete - stop and confirm position
            stop_motion(data);
            data->current_position = data->target_position;
            data->state = FLAP_STATE_IDLE;
        }
    }
}

void flap_controller_set_position(flap_controller_data_t *data, uint8_t position)
{
    if (position > 100U)
    {
        position = 100U;
    }

    if (position == data->target_position)
    {
        return;
    }

    // If motion is in progress, stop and recalculate current position from elapsed time
    if (data->state != FLAP_STATE_IDLE)
    {
        uint32_t elapsed_ms = HAL_GetTick() - data->motion_start_tick;
        if (elapsed_ms > data->motion_duration_ms)
        {
            elapsed_ms = data->motion_duration_ms;
        }

        flap_state_t prev_state = data->state;
        uint32_t travel_time_ms = (prev_state == FLAP_STATE_OPENING) ? data->open_travel_time_ms : data->close_travel_time_ms;
        uint8_t moved = calc_position_change_percent(travel_time_ms, elapsed_ms);

        stop_motion(data);

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
            if (data->current_position >= moved)
            {
                data->current_position -= moved;
            }
            else
            {
                data->current_position = 0U;
            }
        }

        data->state = FLAP_STATE_IDLE;
    }

    data->target_position = position;
}

uint8_t flap_controller_get_position(flap_controller_data_t *data)
{
    return data->current_position;
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
