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

/**
 * @brief UI input handler data structure
 * Manages button state, debouncing, and callback dispatching for user interface
 * Supports 5 buttons: up, down, left, right, ok
 * Features: 50ms debounce, 1000ms long press detection
 */
typedef struct
{
	/**
	 * @brief Callback for left button short press
	 * Typically used to shift focus to previous field
	 */
	void (*shift_focus_left)(void);

	/**
	 * @brief Callback for right button short press
	 * Typically used to shift focus to next field
	 */
	void (*shift_focus_right)(void);

	/**
	 * @brief Callback for up button short press
	 * Typically used to increment focused field value
	 */
	void (*increase_time)(void);

	/**
	 * @brief Callback for down button short press
	 * Typically used to decrement focused field value
	 */
	void (*decrease_time)(void);

	/**
	 * @brief Callback for OK button long press (1000ms)
	 * @param enter true to enter time edit mode, false to exit
	 */
	void (*time_edit_mode)(bool enter);

	// Button state variables
	bool input_up;              ///< Current state of up button (from analog keyboard)
	bool input_down;            ///< Current state of down button
	bool input_left;            ///< Current state of left button
	bool input_right;           ///< Current state of right button
	bool input_ok;              ///< Current state of OK button

	bool input_up_prev;         ///< Previous state of up button (for edge detection)
	bool input_down_prev;       ///< Previous state of down button
	bool input_left_prev;       ///< Previous state of left button
	bool input_right_prev;      ///< Previous state of right button
	bool input_ok_prev;         ///< Previous state of OK button

	// Debounce and timing
	uint32_t up_press_start_time;    ///< Timestamp when up button was first pressed
	uint32_t down_press_start_time;  ///< Timestamp when down button was first pressed
	uint32_t left_press_start_time;  ///< Timestamp when left button was first pressed
	uint32_t right_press_start_time; ///< Timestamp when right button was first pressed
	uint32_t ok_press_start_time;    ///< Timestamp when OK button was first pressed

	// State flags
	bool up_pressed;            ///< True if up button is currently pressed (after debounce)
	bool down_pressed;          ///< True if down button is currently pressed
	bool left_pressed;          ///< True if left button is currently pressed
	bool right_pressed;         ///< True if right button is currently pressed
	bool ok_pressed;            ///< True if OK button is currently pressed
	bool ok_long_press_triggered; ///< True if OK long press has been triggered (prevents repeat)

}ui_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize UI input handler
 * @param this Pointer to UI data structure
 * @param shift_focus_left Callback for left button short press
 * @param shift_focus_right Callback for right button short press
 * @param increase_time Callback for up button short press
 * @param decrease_time Callback for down button short press
 * @param time_edit_mode Callback for OK button long press (bool parameter: enter/exit mode)
 */
void ui_init(ui_data_t * this,
		void (*shift_focus_left)(void),
		void (*shift_focus_right)(void),
		void (*increase_time)(void),
		void (*decrease_time)(void),
		void (*time_edit_mode)(bool enter));

/**
 * @brief Main function - call periodically to process button events
 * Handles debouncing (50ms), edge detection, and callback dispatching
 * Detects long press on OK button (1000ms threshold)
 * @param this Pointer to UI data structure
 */
void ui_main_function(ui_data_t * this);

/**
 * @brief Update up button state
 * Call from analog keyboard callback when up button state changes
 * @param this Pointer to UI data structure
 * @param val true if button is pressed, false if released
 */
void ui_set_input_up(ui_data_t * this, bool val);

/**
 * @brief Update down button state
 * Call from analog keyboard callback when down button state changes
 * @param this Pointer to UI data structure
 * @param val true if button is pressed, false if released
 */
void ui_set_input_down(ui_data_t * this, bool val);

/**
 * @brief Update left button state
 * Call from analog keyboard callback when left button state changes
 * @param this Pointer to UI data structure
 * @param val true if button is pressed, false if released
 */
void ui_set_input_left(ui_data_t * this, bool val);

/**
 * @brief Update right button state
 * Call from analog keyboard callback when right button state changes
 * @param this Pointer to UI data structure
 * @param val true if button is pressed, false if released
 */
void ui_set_input_right(ui_data_t * this, bool val);

/**
 * @brief Update OK button state
 * Call from analog keyboard callback when OK button state changes
 * @param this Pointer to UI data structure
 * @param val true if button is pressed, false if released
 */
void ui_set_input_ok(ui_data_t * this, bool val);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_UI_UI_H_ */
