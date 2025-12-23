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
typedef enum
{
	GUI_FOCUS_NONE,
	GUI_FOCUS_HOUR,
	GUI_FOCUS_MINUTE,
	GUI_FOCUS_SECOND,
	GUI_FOCUS_DAY,
	GUI_FOCUS_MONTH,
	GUI_FOCUS_YEAR
}gui_focus_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void gui_main(void);
void gui_focus(gui_focus_t focus);
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
