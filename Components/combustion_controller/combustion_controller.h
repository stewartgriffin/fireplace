/***********************************************************************************************************************
 *
 *            File: combustion_controller.h
 *      Created on: Feb 26, 2026
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

#ifndef COMPONENTS_COMBUSTION_CONTROLLER_COMBUSTION_CONTROLLER_H_
#define COMPONENTS_COMBUSTION_CONTROLLER_COMBUSTION_CONTROLLER_H_

/**************************************           INCLUDE FILES              ******************************************/
#include <stdint.h>
#include <stdbool.h>

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

typedef enum {
    COMBUSTION_STATE_OFF,
    COMBUSTION_STATE_STARTUP,
    COMBUSTION_STATE_WORKING,
    COMBUSTION_STATE_PROTECTION,
    COMBUSTION_STATE_ENDING,
    COMBUSTION_STATE_COOL_DOWN
} combustion_state_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

void combustion_controller_init(void);
void combustion_controller_main(void);
void combustion_controller_set_exhaust_temperature(int32_t input);
void combustion_controller_startup_requested(void);
void combustion_controller_end_requested(void);
combustion_state_t combustion_controller_get_state(void);
uint8_t combustion_controller_get_flap_position(void);

int16_t combustion_controller_get_dTdt_10s(void);
int16_t combustion_controller_get_dTdt_20s(void);
int16_t combustion_controller_get_dTdt_30s(void);

int16_t combustion_controller_get_sliding_max_dTdt_10s(void);
int16_t combustion_controller_get_sliding_max_dTdt_20s(void);
int16_t combustion_controller_get_sliding_max_dTdt_30s(void);

int16_t combustion_controller_get_session_max_dTdt_10s(void);
int16_t combustion_controller_get_session_max_dTdt_20s(void);
int16_t combustion_controller_get_session_max_dTdt_30s(void);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_COMBUSTION_CONTROLLER_COMBUSTION_CONTROLLER_H_ */
