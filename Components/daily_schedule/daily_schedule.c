/***********************************************************************************************************************
 *
 *            File: daily_schedule.c
 *      Created on: Dec 24, 2024
 *          Author: Claude Code
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "daily_schedule.h"
#include "stm32h5xx_hal.h"

/**************************************           DEFINES                    ******************************************/
#define UPDATE_INTERVAL_MS 1000  // Update every 1 second
#define DUTY_CYCLE_PERIOD_MINUTES 30  // 30-minute duty cycle period

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static uint8_t find_current_value(const daily_schedule_config_t *config, uint8_t hour, uint8_t minute);
static bool calculate_enable_state(const daily_schedule_config_t *config, uint8_t percentage, uint8_t hour, uint8_t minute);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/

void daily_schedule_init(daily_schedule_data_t *data,
						 const daily_schedule_config_t *config)
{
	data->config = config;
	data->current_value = 0;
	data->current_enable = false;
	data->last_update_tick = HAL_GetTick();
}

void daily_schedule_main(daily_schedule_data_t *data, time_data_t current_time)
{
	uint32_t current_tick = HAL_GetTick();
	uint32_t elapsed_ms = current_tick - data->last_update_tick;

	// Update only if sufficient time has passed
	if (elapsed_ms < UPDATE_INTERVAL_MS)
	{
		return;
	}

	data->last_update_tick = current_tick;

	// If no config, nothing to do
	if (data->config == NULL)
	{
		return;
	}

	// Find and update current value based on schedule
	data->current_value = find_current_value(data->config, current_time.hour, current_time.minute);

	// Calculate enable state based on duty cycle
	data->current_enable = calculate_enable_state(data->config, data->current_value, current_time.hour, current_time.minute);
}

uint8_t daily_schedule_get_value(daily_schedule_data_t *data)
{
	return data->current_value;
}

bool daily_schedule_get_enable(daily_schedule_data_t *data)
{
	return data->current_enable;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/

/**
 * @brief Find the current scheduled value based on time
 * @param config Pointer to schedule configuration
 * @param hour Current hour (0-23)
 * @param minute Current minute (0-59)
 * @return Scheduled value (0-100%)
 */
static uint8_t find_current_value(const daily_schedule_config_t *config, uint8_t hour, uint8_t minute)
{
	// Convert current time to minutes since midnight
	uint16_t current_minutes = (uint16_t)hour * 60 + minute;

	// Find the most recent entry that has passed
	uint8_t active_value = 0;
	uint16_t best_time = 0;

	for (uint8_t i = 0; i < config->num_entries; i++)
	{
		uint16_t entry_minutes = (uint16_t)config->entries[i].hour * 60 + config->entries[i].minute;

		// If this entry's time has passed and it's more recent than our current best
		if (entry_minutes <= current_minutes && entry_minutes >= best_time)
		{
			best_time = entry_minutes;
			active_value = config->entries[i].value;
		}
	}

	return active_value;
}

/**
 * @brief Calculate enable state based on 30-minute duty cycle
 * @param config Pointer to schedule configuration (unused, for future extensions)
 * @param percentage Current scheduled percentage (0-100%)
 * @param hour Current hour (0-23)
 * @param minute Current minute (0-59)
 * @return true during active portion of duty cycle, false otherwise
 */
static bool calculate_enable_state(const daily_schedule_config_t *config, uint8_t percentage, uint8_t hour, uint8_t minute)
{
	// Unused parameter
	(void)config;

	// Calculate which minute we are within the current 30-minute block (0-29)
	uint8_t minute_within_block = minute % DUTY_CYCLE_PERIOD_MINUTES;

	// Calculate threshold: how many minutes should be "true" in this 30-minute block
	// threshold_minutes = (30 * percentage) / 100
	uint8_t threshold_minutes = (DUTY_CYCLE_PERIOD_MINUTES * percentage) / 100;

	// Return true if we're within the active portion of the duty cycle
	return (minute_within_block < threshold_minutes);
}
