/***********************************************************************************************************************
 *
 *            File: nvm_manager.h
 *      Created on: Mar 15, 2026
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef COMPONENTS_NVM_MANAGER_NVM_MANAGER_H_
#define COMPONENTS_NVM_MANAGER_NVM_MANAGER_H_

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"
#include "ds3231.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief NVM manager driver data structure.
 * Persists selected values to internal flash and syncs them to RAM on startup.
 * Flash is only written once per day at midnight to minimize erase cycles.
 */
typedef struct
{
    /**
     * @brief Function pointer to retrieve current RTC time.
     * @return Pointer to current time_data_t; must remain valid for the duration of the call.
     */
    time_data_t *(*get_time)(void);

    /* Internal state — do not access directly */
    uint8_t last_saved_hour; ///< Hour value at the last save, used to detect midnight transition
} nvm_manager_data_t;

/**************************************           PUBLIC FUNCTIONS           ******************************************/

/**
 * @brief Initialise NVM manager and load persisted values from flash into RAM.
 * @param nvm Pointer to nvm_manager_data_t instance.
 */
void nvm_manager_init(nvm_manager_data_t *nvm);

/**
 * @brief Periodic NVM manager update. Saves RAM values to flash once at midnight (00:00).
 * @param nvm Pointer to nvm_manager_data_t instance.
 */
void nvm_manager_main(nvm_manager_data_t *nvm);

/**
 * @brief Store a new highest exhaust temperature value in RAM.
 *        The value is persisted to flash at midnight.
 * @param temp Temperature in degrees Celsius (int16_t).
 */
void nvm_manager_set_highest_exhaust_temperature(int16_t temp);

/**
 * @brief Return the highest exhaust temperature recorded since last reset/clear.
 *        On startup the value is loaded from flash.
 * @return Temperature in degrees Celsius.
 */
int16_t nvm_manager_get_highest_exhaust_temperature(void);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_NVM_MANAGER_NVM_MANAGER_H_ */
