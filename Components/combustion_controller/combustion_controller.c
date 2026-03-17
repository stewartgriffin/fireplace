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

#define HYSTERESIS 7

#define STARTUP_GRACE_PERIOD_MS       (30U * 60U * 1000U)
#define END_DELAY_MS                  (90U * 60U * 1000U)

#define DRAFT_SUPPRESSION_DTDT_THRESHOLD  9
#define DRAFT_SUPPRESSION_TEMP_THRESHOLD  80
#define DRAFT_SUPPRESSION_DURATION_MS     (2U * 60U * 1000U)

#define WORKING_TARGET_TEMP               120
#define WORKING_SESSION_RESET_TEMP        113
#define WORKING_ADJUST_INTERVAL_MS        (30U * 1000U)

#define TEMP_BUFFER_SIZE              300

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
static int32_t temperature          = 0;
static combustion_state_t state     = COMBUSTION_STATE_OFF;
static bool startup_requested       = false;
static bool end_requested           = false;
static uint32_t startup_tick        = 0;
static uint32_t end_tick            = 0;

static int16_t  temp_buf[TEMP_BUFFER_SIZE] = {0};
static int      buf_head            = 0;
static uint32_t last_sample_tick;   // set to HAL_GetTick() in init
static bool     buf_primed          = false;
static int16_t  session_max_10s     = 0;
static int16_t  session_max_20s     = 0;
static int16_t  session_max_30s     = 0;

static bool     draft_suppression_active = false;
static uint32_t draft_suppression_tick   = 0U;

static void (*swipe_open_cb)(void);
static void (*swipe_close_cb)(void);
static void (*set_flap_position_cb)(uint8_t);
static uint32_t working_adjust_tick    = 0U;
static bool     working_session_active = false;

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static int16_t calc_dTdt(int window);
static int16_t calc_sliding_max_dTdt(int window);
static void    sample_temperature(void);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void combustion_controller_init(void (*swipe_open)(void), void (*swipe_close)(void), void (*set_flap_position)(uint8_t))
{
    swipe_open_cb        = swipe_open;
    swipe_close_cb       = swipe_close;
    set_flap_position_cb = set_flap_position;
    last_sample_tick     = HAL_GetTick();
}

void combustion_controller_main(void)
{
    sample_temperature();

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

    // Compute startup grace status
    bool startup_grace_active = (startup_tick != 0) && ((HAL_GetTick() - startup_tick) < STARTUP_GRACE_PERIOD_MS);

    // After 90 minutes, force transition to ENDING from any active burning state
    if (end_tick != 0 && (HAL_GetTick() - end_tick) >= END_DELAY_MS)
    {
        if (state == COMBUSTION_STATE_STARTUP || state == COMBUSTION_STATE_WORKING)
        {
            startup_tick             = 0;
            draft_suppression_active = false;
            working_session_active   = false;
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
        if (set_flap_position_cb != NULL) { set_flap_position_cb(0); }
    }
    else if (state == COMBUSTION_STATE_STARTUP)
    {
        if (temperature > WORKING_ENTER_THRESHOLD_TEMP)
        {
            startup_tick           = 0;
            working_adjust_tick    = 0U;
            working_session_active = false;
            state = COMBUSTION_STATE_WORKING;
        }
        else if (!startup_grace_active && temperature < STARTUP_ENTER_THRESHOLD_TEMP - HYSTERESIS)
        {
            startup_tick = 0;
            state = COMBUSTION_STATE_OFF;
        }
        if (set_flap_position_cb != NULL) { set_flap_position_cb(100); }
    }
    else if (state == COMBUSTION_STATE_WORKING)
    {
        // Expire draft suppression after 2 minutes; reopen flap to resume normal operation
        if (draft_suppression_active &&
            (HAL_GetTick() - draft_suppression_tick) >= DRAFT_SUPPRESSION_DURATION_MS)
        {
            draft_suppression_active = false;
            // Session active: issue a swipe_open to start recovering; pre-session case is handled
            // by set_flap_position_cb(100) being called each cycle until the first 120°C crossing.
            if (working_session_active && swipe_open_cb != NULL) { swipe_open_cb(); }
            working_adjust_tick = HAL_GetTick();
        }

        // Session reset: temperature fell back below 105°C — flap back to 100%
        if (temperature < WORKING_SESSION_RESET_TEMP)
        {
            working_session_active = false;
        }

        // Draft suppression: close flap for 2 min when dT/dt 20s >= 9 °C/min and temp > 80°C
        if (!draft_suppression_active &&
            calc_dTdt(20) >= DRAFT_SUPPRESSION_DTDT_THRESHOLD &&
            temperature > DRAFT_SUPPRESSION_TEMP_THRESHOLD)
        {
            draft_suppression_active = true;
            draft_suppression_tick   = HAL_GetTick();
        }

        // 120°C crossing: close flap for 30s then balance every 30s
        if (!working_session_active && temperature >= WORKING_TARGET_TEMP)
        {
            working_session_active = true;
            working_adjust_tick    = HAL_GetTick();
            if (swipe_close_cb != NULL) { swipe_close_cb(); }
        }
        // 30-second adjustment — only active once balance mode is running
        else if (working_session_active &&
                 (HAL_GetTick() - working_adjust_tick) >= WORKING_ADJUST_INTERVAL_MS)
        {
            working_adjust_tick = HAL_GetTick();
            if (temperature > WORKING_TARGET_TEMP)
            {
                if (swipe_close_cb != NULL) { swipe_close_cb(); }
            }
            else if (temperature < WORKING_TARGET_TEMP)
            {
                if (swipe_open_cb != NULL) { swipe_open_cb(); }
            }
        }

        if (draft_suppression_active)
        {
            if (set_flap_position_cb != NULL) { set_flap_position_cb(0); }
        }
        else if (!working_session_active)
        {
            if (set_flap_position_cb != NULL) { set_flap_position_cb(100); }
        }

        if (temperature < ENDING_ENTER_THRESHOLD_TEMP)
        {
            draft_suppression_active = false;
            working_session_active   = false;
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
        if (set_flap_position_cb != NULL) { set_flap_position_cb(30); }
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
        if (set_flap_position_cb != NULL) { set_flap_position_cb(0); }
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

/* Sample exhaust temperature into the circular buffer once per second.
 * On first valid reading, primes the buffer with the current temperature.
 * Updates session-maximum dTdt values each second. */
static void sample_temperature(void)
{
    if (HAL_GetTick() - last_sample_tick < 1000U)
        return;

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
        return;
    }

    temp_buf[buf_head] = (int16_t)temperature;
    buf_head = (buf_head + 1) % TEMP_BUFFER_SIZE;

    int16_t dt10 = calc_dTdt(10);
    int16_t dt20 = calc_dTdt(20);
    int16_t dt30 = calc_dTdt(30);
    if (dt10 > session_max_10s) session_max_10s = dt10;
    if (dt20 > session_max_20s) session_max_20s = dt20;
    if (dt30 > session_max_30s) session_max_30s = dt30;
}

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

