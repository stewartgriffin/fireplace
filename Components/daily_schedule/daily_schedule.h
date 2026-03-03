/***********************************************************************************************************************
 *
 *            File: daily_schedule.h
 *      Created on: Dec 24, 2024
 *          Author: Claude Code
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

#ifndef COMPONENTS_DAILY_SCHEDULE_DAILY_SCHEDULE_H_
#define COMPONENTS_DAILY_SCHEDULE_DAILY_SCHEDULE_H_

/**************************************           INCLUDE FILES              ******************************************/
#include <stdint.h>
#include <stdbool.h>
#include "ds3231.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief Single schedule entry with time, duty cycle, and level
 * Defines when a particular setting should be applied during the day
 */
typedef struct {
	uint8_t hour;        // Hour (0-23)
	uint8_t minute;      // Minute (0-59)
	uint8_t duty_cycle;  // Duty cycle percentage (0-100%) — controls how often enable is active
	uint8_t level;       // Output level associated with this period (application-defined)
} daily_schedule_entry_t;

/**
 * @brief Schedule configuration (defined by user at integration point)
 * Example usage in fireplace.c:
 *
 * static const daily_schedule_entry_t my_schedule[] = {
 *     {0, 0, 0, 0},      // Midnight: 0% duty cycle, level 0
 *     {6, 0, 50, 1},     // 6:00 AM: 50% duty cycle, level 1
 *     {8, 0, 100, 2},    // 8:00 AM: 100% duty cycle, level 2
 *     {22, 0, 0, 0},     // 10:00 PM: 0% duty cycle, level 0
 * };
 *
 * static const daily_schedule_config_t my_config = {
 *     .entries = my_schedule,
 *     .num_entries = sizeof(my_schedule) / sizeof(my_schedule[0])
 * };
 */
typedef struct {
	const daily_schedule_entry_t *entries;  // Pointer to array of schedule entries
	uint8_t num_entries;                    // Number of entries in the array
} daily_schedule_config_t;

/**
 * @brief Result returned by daily_schedule_get()
 */
typedef struct {
	bool enable;      // true during the active portion of the duty cycle
	uint8_t level;    // Level from the currently active schedule entry
} daily_schedule_result_t;

/**
 * @brief Daily schedule runtime data structure
 */
typedef struct {
	// Configuration (set by user)
	const daily_schedule_config_t *config;

	// Runtime state
	uint8_t current_duty_cycle;  // Duty cycle % of the active entry (0-100%)
	uint8_t current_level;       // Level of the active entry
	bool current_enable;         // Enable state based on duty cycle
	uint32_t last_update_tick;
} daily_schedule_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize daily schedule
 * @param data Pointer to daily schedule data structure
 * @param config Pointer to schedule configuration (must remain valid)
 */
void daily_schedule_init(daily_schedule_data_t *data,
						 const daily_schedule_config_t *config);

/**
 * @brief Main function - call periodically to update schedule
 * Updates both the enable state and level based on time and duty cycle
 * @param data Pointer to daily schedule data structure
 * @param current_time Current time from DS3231
 */
void daily_schedule_main(daily_schedule_data_t *data, time_data_t current_time);

/**
 * @brief Get current schedule result
 * Returns the enable state (from duty cycle) and level of the active schedule entry.
 * For example with 10% duty cycle over a 20-minute period: enable is true for 2 minutes out of every 20 minutes.
 * @param data Pointer to daily schedule data structure
 * @return Struct containing enable (bool) and level (uint8_t)
 */
daily_schedule_result_t daily_schedule_get(daily_schedule_data_t *data);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_DAILY_SCHEDULE_DAILY_SCHEDULE_H_ */
