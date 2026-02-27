/***********************************************************************************************************************
 *
 *            File: combustion_controller.c
 *      Created on: Feb 26, 2026
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "combustion_controller.h"
#include "stm32h5xx_hal.h"

/**************************************           DEFINES                    ******************************************/
#define STARTUP_ENTER_THRESHOLD_TEMP 38
#define WORKING_ENTER_THRESHOLD_TEMP 80
#define ENDING_ENTER_THRESHOLD_TEMP 70
#define COOL_DOWN_ENTER_THRESHOLD_TEMP 50
#define PROTECTION_ENTER_THRESHOLD_TEMP 400
#define PROTECTION_EXIT_THRESHOLD_TEMP 350

#define HYSTERESIS 7

#define STARTUP_GRACE_PERIOD_MS (30U * 60U * 1000U)
#define END_DELAY_MS            (90U * 60U * 1000U)

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
int32_t temperature;
combustion_state_t state;
bool startup_requested;
bool end_requested;
uint32_t startup_tick;
uint32_t end_tick;

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void combustion_controller_init(void)
{
    temperature = 0;
    state = COMBUSTION_STATE_OFF;
    startup_requested = false;
    end_requested = false;
    startup_tick = 0;
    end_tick = 0;
}

void combustion_controller_main(void)
{
    // Process flags first - applies from any state
    if (startup_requested)
    {
        startup_requested = false;
        startup_tick = HAL_GetTick();
        if (state == COMBUSTION_STATE_OFF || state == COMBUSTION_STATE_ENDING || state == COMBUSTION_STATE_COOL_DOWN)
        {
            state = COMBUSTION_STATE_STARTUP;
        }
    }
    if (end_requested && end_tick == 0)
    {
        end_tick = HAL_GetTick();
    }

    if (state == COMBUSTION_STATE_PROTECTION)
    {
        if (temperature < PROTECTION_EXIT_THRESHOLD_TEMP)
        {
            state = COMBUSTION_STATE_WORKING;
        }
        return;
    }

    // Protection threshold overrides any state
    if (temperature > PROTECTION_ENTER_THRESHOLD_TEMP)
    {
        state = COMBUSTION_STATE_PROTECTION;
        return;
    }

    // Compute startup grace status
    bool startup_grace_active = (startup_tick != 0) && ((HAL_GetTick() - startup_tick) < STARTUP_GRACE_PERIOD_MS);

    // After 90 minutes, force transition to ENDING from any active burning state
    if (end_tick != 0 && (HAL_GetTick() - end_tick) >= END_DELAY_MS)
    {
        if (state == COMBUSTION_STATE_STARTUP || state == COMBUSTION_STATE_WORKING)
        {
            startup_tick = 0;
            state = COMBUSTION_STATE_ENDING;
        }
    }

    if (state == COMBUSTION_STATE_OFF)
    {
        if (temperature > STARTUP_ENTER_THRESHOLD_TEMP)
        {
            state = COMBUSTION_STATE_STARTUP;
        }
    }
    else if (state == COMBUSTION_STATE_STARTUP)
    {
        if (!startup_grace_active)
        {
            if (temperature > WORKING_ENTER_THRESHOLD_TEMP)
            {
                startup_tick = 0;
                state = COMBUSTION_STATE_WORKING;
            }
            else if (temperature < STARTUP_ENTER_THRESHOLD_TEMP - HYSTERESIS)
            {
                startup_tick = 0;
                state = COMBUSTION_STATE_OFF;
            }
        }
    }
    else if (state == COMBUSTION_STATE_WORKING)
    {
        if (temperature < ENDING_ENTER_THRESHOLD_TEMP)
        {
            state = COMBUSTION_STATE_ENDING;
        }
    }
    else if (state == COMBUSTION_STATE_ENDING)
    {
        if (temperature < COOL_DOWN_ENTER_THRESHOLD_TEMP)
        {
            state = COMBUSTION_STATE_COOL_DOWN;
            if (end_requested && end_tick != 0)
            {
                end_requested = false;
                end_tick = 0;
            }
        }
        if (!end_requested && temperature > ENDING_ENTER_THRESHOLD_TEMP + HYSTERESIS)
        {
            state = COMBUSTION_STATE_WORKING;
        }
    }
    else if (state == COMBUSTION_STATE_COOL_DOWN)
    {
        if (temperature < STARTUP_ENTER_THRESHOLD_TEMP - HYSTERESIS)
        {
            state = COMBUSTION_STATE_OFF;
        }
        if (temperature > COOL_DOWN_ENTER_THRESHOLD_TEMP + HYSTERESIS)
        {
            state = COMBUSTION_STATE_ENDING;
        }
    }
}

void combustion_controller_set_exhaust_temperature(int32_t input)
{
    temperature = input;
}

void combustion_controller_startup_requested(void)
{
    startup_requested = true;
    // Cancel any pending end request - last request wins
    end_requested = false;
    end_tick = 0;
}

void combustion_controller_end_requested(void)
{
    end_requested = true;
}

combustion_state_t combustion_controller_get_state(void)
{
    return state;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
