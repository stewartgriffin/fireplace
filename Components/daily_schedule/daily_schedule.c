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
#define DUTY_CYCLE_PERIOD_MINUTES 20  // 20-minute duty cycle period

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static const daily_schedule_entry_t *find_current_entry(const daily_schedule_config_t *config, uint8_t hour, uint8_t minute);
static bool calculate_enable_state(const daily_schedule_config_t *config, uint8_t duty_cycle, uint8_t hour, uint8_t minute);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/

void daily_schedule_init(daily_schedule_data_t *data,
						 const daily_schedule_config_t *config)
{
	data->config = config;
	data->current_duty_cycle = 0;
	data->current_level = 0;
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

	// Find the active entry and extract duty cycle and level
	const daily_schedule_entry_t *entry = find_current_entry(data->config, current_time.hour, current_time.minute);
	if (entry != NULL)
	{
		data->current_duty_cycle = entry->duty_cycle;
		data->current_level = entry->level;
	}
	else
	{
		data->current_duty_cycle = 0;
		data->current_level = 0;
	}

	// Calculate enable state based on duty cycle
	data->current_enable = calculate_enable_state(data->config, data->current_duty_cycle, current_time.hour, current_time.minute);
}

daily_schedule_result_t daily_schedule_get(daily_schedule_data_t *data)
{
	daily_schedule_result_t result;
	result.enable = data->current_enable;
	result.level = data->current_level;
	return result;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/

/**
 * @brief Find the active schedule entry based on current time
 * Returns the most recent entry whose time has passed, or NULL if none found.
 * @param config Pointer to schedule configuration
 * @param hour Current hour (0-23)
 * @param minute Current minute (0-59)
 * @return Pointer to the active entry, or NULL if no entry has passed yet
 */
static const daily_schedule_entry_t *find_current_entry(const daily_schedule_config_t *config, uint8_t hour, uint8_t minute)
{
	// Convert current time to minutes since midnight
	uint16_t current_minutes = (uint16_t)hour * 60 + minute;

	// Find the most recent entry that has passed
	const daily_schedule_entry_t *best_entry = NULL;
	uint16_t best_time = 0;

	for (uint8_t i = 0; i < config->num_entries; i++)
	{
		uint16_t entry_minutes = (uint16_t)config->entries[i].hour * 60 + config->entries[i].minute;

		// If this entry's time has passed and it's more recent than our current best
		if (entry_minutes <= current_minutes && entry_minutes >= best_time)
		{
			best_time = entry_minutes;
			best_entry = &config->entries[i];
		}
	}

	return best_entry;
}

/**
 * @brief Calculate enable state based on duty cycle within the period
 * @param config Pointer to schedule configuration (unused, for future extensions)
 * @param duty_cycle Current duty cycle percentage (0-100%)
 * @param hour Current hour (0-23)
 * @param minute Current minute (0-59)
 * @return true during active portion of duty cycle, false otherwise
 */
static bool calculate_enable_state(const daily_schedule_config_t *config, uint8_t duty_cycle, uint8_t hour, uint8_t minute)
{
	// Unused parameters
	(void)config;
	(void)hour;

	// Calculate which minute we are within the current period block
	uint8_t minute_within_block = minute % DUTY_CYCLE_PERIOD_MINUTES;

	// Calculate threshold: how many minutes should be "true" in this block
	uint8_t threshold_minutes = (DUTY_CYCLE_PERIOD_MINUTES * duty_cycle) / 100;

	// Return true if we're within the active portion of the duty cycle
	return (minute_within_block < threshold_minutes);
}
