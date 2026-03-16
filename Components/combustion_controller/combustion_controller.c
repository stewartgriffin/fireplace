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
#define STARTUP_ENTER_THRESHOLD_TEMP 37
#define WORKING_ENTER_THRESHOLD_TEMP 80
#define ENDING_ENTER_THRESHOLD_TEMP 70
#define COOL_DOWN_ENTER_THRESHOLD_TEMP 50
#define PROTECTION_ENTER_THRESHOLD_TEMP 120
#define PROTECTION_MAX_ALLOWED_TEMP     130

#define HYSTERESIS 7

#define STARTUP_GRACE_PERIOD_MS       (30U * 60U * 1000U)
#define END_DELAY_MS                  (90U * 60U * 1000U)
#define PROTECTION_FLAP_UPDATE_MS     (60U * 1000U)

#define TEMP_BUFFER_SIZE              300

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
static int32_t temperature;
static combustion_state_t state;
static bool startup_requested;
static bool end_requested;
static uint32_t startup_tick;
static uint32_t end_tick;
static uint8_t  protection_flap;
static uint32_t protection_flap_tick;

static int16_t  temp_buf[TEMP_BUFFER_SIZE];
static int      buf_head;
static uint32_t last_sample_tick;
static bool     buf_primed;
static int16_t  session_max_10s;
static int16_t  session_max_20s;
static int16_t  session_max_30s;

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static uint8_t calc_protection_flap(void);
static int16_t calc_dTdt(int window);
static int16_t calc_sliding_max_dTdt(int window);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void combustion_controller_init(void)
{
    temperature = 0;
    state = COMBUSTION_STATE_OFF;
    startup_requested = false;
    end_requested = false;
    startup_tick = 0;
    end_tick = 0;
    protection_flap = 100U;
    protection_flap_tick = 0U;

    buf_head = 0;
    last_sample_tick = HAL_GetTick();
    buf_primed = false;
    session_max_10s = 0;
    session_max_20s = 0;
    session_max_30s = 0;
}

void combustion_controller_main(void)
{
    // Sample exhaust temperature into circular buffer once per second
    if (HAL_GetTick() - last_sample_tick >= 1000U)
    {
        last_sample_tick += 1000U;
        if (!buf_primed)
        {
            if (temperature != 0)
            {
                for (int i = 0; i < TEMP_BUFFER_SIZE; i++)
                    temp_buf[i] = (int16_t)temperature;
                buf_head = 0;
                buf_primed = true;
            }
        }
        else
        {
            temp_buf[buf_head] = (int16_t)temperature;
            buf_head = (buf_head + 1) % TEMP_BUFFER_SIZE;
            int16_t dt10 = calc_dTdt(10);
            int16_t dt20 = calc_dTdt(20);
            int16_t dt30 = calc_dTdt(30);
            if (dt10 > session_max_10s) session_max_10s = dt10;
            if (dt20 > session_max_20s) session_max_20s = dt20;
            if (dt30 > session_max_30s) session_max_30s = dt30;
        }
    }

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
        if (temperature < PROTECTION_ENTER_THRESHOLD_TEMP - HYSTERESIS)
        {
            state = COMBUSTION_STATE_WORKING;
        }
        else if (protection_flap_tick == 0U || (HAL_GetTick() - protection_flap_tick) >= PROTECTION_FLAP_UPDATE_MS)
        {
            protection_flap = calc_protection_flap();
            protection_flap_tick = HAL_GetTick();
        }
        return;
    }

    // Protection threshold overrides any state
    if (temperature > PROTECTION_ENTER_THRESHOLD_TEMP)
    {
        state = COMBUSTION_STATE_PROTECTION;
        protection_flap_tick = 0U;
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
            startup_tick = HAL_GetTick();
            state = COMBUSTION_STATE_STARTUP;
        }
    }
    else if (state == COMBUSTION_STATE_STARTUP)
    {
        if (temperature > WORKING_ENTER_THRESHOLD_TEMP)
        {
            startup_tick = 0;
            state = COMBUSTION_STATE_WORKING;
        }
        else if (!startup_grace_active && temperature < STARTUP_ENTER_THRESHOLD_TEMP - HYSTERESIS)
        {
            startup_tick = 0;
            state = COMBUSTION_STATE_OFF;
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
            session_max_10s = 0;
            session_max_20s = 0;
            session_max_30s = 0;
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

uint8_t combustion_controller_get_flap_position(void)
{
    switch (state)
    {
        case COMBUSTION_STATE_OFF:       return 0;
        case COMBUSTION_STATE_STARTUP:   return 100;
        case COMBUSTION_STATE_WORKING:   return 100;
        case COMBUSTION_STATE_ENDING:    return 30;
        case COMBUSTION_STATE_COOL_DOWN: return 0;
        case COMBUSTION_STATE_PROTECTION: return protection_flap;
        default: return 0;
    }
}

int16_t combustion_controller_get_dTdt_10s(void)
{
    return calc_dTdt(10);
}

int16_t combustion_controller_get_dTdt_20s(void)
{
    return calc_dTdt(20);
}

int16_t combustion_controller_get_dTdt_30s(void)
{
    return calc_dTdt(30);
}

int16_t combustion_controller_get_sliding_max_dTdt_10s(void)
{
    return calc_sliding_max_dTdt(10);
}

int16_t combustion_controller_get_sliding_max_dTdt_20s(void)
{
    return calc_sliding_max_dTdt(20);
}

int16_t combustion_controller_get_sliding_max_dTdt_30s(void)
{
    return calc_sliding_max_dTdt(30);
}

int16_t combustion_controller_get_session_max_dTdt_10s(void)
{
    return session_max_10s;
}

int16_t combustion_controller_get_session_max_dTdt_20s(void)
{
    return session_max_20s;
}

int16_t combustion_controller_get_session_max_dTdt_30s(void)
{
    return session_max_30s;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/

/* Derivative of temperature over `window` seconds, in °C/min.
 * Uses the circular buffer: newest sample vs. the sample `window` seconds ago.
 * Returns 0 if the buffer has not been primed yet. */
static int16_t calc_dTdt(int window)
{
    if (!buf_primed)
        return 0;
    int newest = (buf_head - 1 + TEMP_BUFFER_SIZE) % TEMP_BUFFER_SIZE;
    int older  = (buf_head - 1 - window + TEMP_BUFFER_SIZE) % TEMP_BUFFER_SIZE;
    int32_t dT = (int32_t)temp_buf[newest] - (int32_t)temp_buf[older];
    return (int16_t)(dT * 60 / window);
}

/* Highest dTdt (°C/min) over any `window`-second window currently in the buffer.
 * Only positive rates are tracked (temperature rises). Returns 0 if not primed. */
static int16_t calc_sliding_max_dTdt(int window)
{
    if (!buf_primed)
        return 0;
    int16_t max_val = 0;
    for (int j = 0; j <= TEMP_BUFFER_SIZE - 1 - window; j++)
    {
        int newest_idx = (buf_head - 1 - j               + TEMP_BUFFER_SIZE) % TEMP_BUFFER_SIZE;
        int older_idx  = (buf_head - 1 - j - window + TEMP_BUFFER_SIZE) % TEMP_BUFFER_SIZE;
        int32_t dT = (int32_t)temp_buf[newest_idx] - (int32_t)temp_buf[older_idx];
        int16_t dTdt = (int16_t)(dT * 60 / window);
        if (dTdt > max_val)
            max_val = dTdt;
    }
    return max_val;
}

/* Linear interpolation: PROTECTION_ENTER_THRESHOLD_TEMP -> 100%, PROTECTION_MAX_ALLOWED_TEMP -> 20%.
 * Result rounded to the nearest 10%. */
static uint8_t calc_protection_flap(void)
{
    int32_t flap;
    if (temperature <= PROTECTION_ENTER_THRESHOLD_TEMP)
    {
        flap = 100;
    }
    else if (temperature >= PROTECTION_MAX_ALLOWED_TEMP)
    {
        flap = 20;
    }
    else
    {
        int32_t range = PROTECTION_MAX_ALLOWED_TEMP - PROTECTION_ENTER_THRESHOLD_TEMP;
        flap = 100 - ((temperature - PROTECTION_ENTER_THRESHOLD_TEMP) * 80) / range;
    }
    return (uint8_t)(((flap + 5) / 10) * 10);
}
