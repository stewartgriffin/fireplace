/***********************************************************************************************************************
 *
 *            File: gui.h
 *      Created on: Dec 22, 2025
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef SRC_DRIVERS_GUI_GUI_H_
#define SRC_DRIVERS_GUI_GUI_H_

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"
#include "ds3231.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void gui_set_time(time_data_t * time);
void gui_set_fireplace(uint8_t value);
void gui_set_ventilation(uint8_t value);
void gui_set_ppm(uint8_t value);
void gui_set_fireplace_temperature(uint8_t value);
char * gui_get_screen_buffer(void);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_GUI_GUI_H_ */
