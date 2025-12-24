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
 * @brief Single schedule entry with time and value
 * Defines when a particular setting should be applied during the day
 */
typedef struct {
	uint8_t hour;        // Hour (0-23)
	uint8_t minute;      // Minute (0-59)
	uint8_t value;       // Value to apply (0-100%)
} daily_schedule_entry_t;

/**
 * @brief Schedule configuration (defined by user at integration point)
 * Example usage in fireplace.c:
 *
 * static const daily_schedule_entry_t my_schedule[] = {
 *     {0, 0, 0},      // Midnight: 0%
 *     {6, 0, 50},     // 6:00 AM: 50%
 *     {8, 0, 100},    // 8:00 AM: 100%
 *     {22, 0, 0},     // 10:00 PM: 0%
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
 * @brief Daily schedule runtime data structure
 */
typedef struct {
	// Configuration (set by user)
	const daily_schedule_config_t *config;

	// Runtime state
	uint8_t current_value;    // Current scheduled value (0-100%)
	bool current_enable;      // Current enable state based on duty cycle
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
 * Updates both the current value and enable state based on time and duty cycle
 * @param data Pointer to daily schedule data structure
 * @param current_time Current time from DS3231
 */
void daily_schedule_main(daily_schedule_data_t *data, time_data_t current_time);

/**
 * @brief Get current scheduled value
 * @param data Pointer to daily schedule data structure
 * @return Current scheduled value (0-100%)
 */
uint8_t daily_schedule_get_value(daily_schedule_data_t *data);

/**
 * @brief Get schedule enable state based on 30-minute duty cycle
 * The percentage value controls a 30-minute duty cycle pattern.
 * For example, 10% means this returns true for 3 minutes out of every 30 minutes.
 * @param data Pointer to daily schedule data structure
 * @return true during the active portion of the duty cycle, false otherwise
 */
bool daily_schedule_get_enable(daily_schedule_data_t *data);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_DAILY_SCHEDULE_DAILY_SCHEDULE_H_ */
