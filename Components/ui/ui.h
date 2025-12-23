/***********************************************************************************************************************
 *
 *            File: ui.h
 *      Created on: Dec 23, 2025
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved.
 *
***********************************************************************************************************************/

#ifndef COMPONENTS_UI_UI_H_
#define COMPONENTS_UI_UI_H_

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/
typedef struct
{
	// Callback function pointers
	void (*short_press_up)(void);
	void (*short_press_down)(void);
	void (*short_press_left)(void);
	void (*short_press_right)(void);
	void (*long_press_left_and_right)(void);

	// Button state variables
	bool input_up;
	bool input_down;
	bool input_left;
	bool input_right;

	bool input_up_prev;
	bool input_down_prev;
	bool input_left_prev;
	bool input_right_prev;

	// Debounce and timing
	uint32_t up_press_start_time;
	uint32_t down_press_start_time;
	uint32_t left_press_start_time;
	uint32_t right_press_start_time;
	uint32_t left_right_press_start_time;

	// State flags
	bool up_pressed;
	bool down_pressed;
	bool left_pressed;
	bool right_pressed;
	bool left_and_right_pressed;
	bool long_press_triggered;

}ui_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void ui_init(ui_data_t * this,
		void (*short_press_up)(void),
		void (*short_press_down)(void),
		void (*short_press_left)(void),
		void (*short_press_right)(void),
		void (*long_press_left_and_right)(void));

void ui_main_function(ui_data_t * this);

void ui_set_input_up(ui_data_t * this, bool val);
void ui_set_input_down(ui_data_t * this, bool val);
void ui_set_input_left(ui_data_t * this, bool val);
void ui_set_input_right(ui_data_t * this, bool val);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_UI_UI_H_ */
