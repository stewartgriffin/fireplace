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

/**
 * @brief GUI focus enumeration for time/date field selection
 * Defines which field is currently focused for editing
 */
typedef enum
{
	GUI_FOCUS_NONE,     ///< No field focused
	GUI_FOCUS_HOUR,     ///< Hour field focused
	GUI_FOCUS_MINUTE,   ///< Minute field focused
	GUI_FOCUS_SECOND,   ///< Second field focused
	GUI_FOCUS_DAY,      ///< Day field focused
	GUI_FOCUS_MONTH,    ///< Month field focused
	GUI_FOCUS_YEAR      ///< Year field focused
}gui_focus_t;

/**************************************           DEFINES                    ******************************************/
#define GUI_SCREEN_SIZE 80  ///< Total screen size (rows * columns)

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize GUI module
 * @param update_partial_screen Callback for partial screen updates
 *        Returns 0 on success, -1 if display is busy
 */
void gui_init(int (*update_partial_screen)(const char *buffer, uint16_t position, uint16_t length));

/**
 * @brief Main GUI update function
 * Call periodically to update blinking, compare buffers, and send changes to display
 * Automatically handles display busy status and retries
 */
void gui_main(void);

/**
 * @brief Shift focus to the previous time/date field
 * Cycles through fields: YEAR -> MONTH -> DAY -> HOUR -> MINUTE -> SECOND
 */
void gui_shift_focus_left(void);

/**
 * @brief Shift focus to the next time/date field
 * Cycles through fields: SECOND -> MINUTE -> HOUR -> DAY -> MONTH -> YEAR
 */
void gui_shift_focus_right(void);

/**
 * @brief Enable or disable time edit mode
 * When enabled, allows field focus and blinking for time editing
 * @param enable true to enter edit mode, false to exit
 */
void gui_time_edit_mode(bool enable);

/**
 * @brief Get current focused field
 * @return Currently focused field (GUI_FOCUS_NONE if not in edit mode)
 */
gui_focus_t gui_get_focus(void);

/**
 * @brief Update GUI with current time
 * @param time Pointer to time structure from DS3231
 */
void gui_set_time(time_data_t * time);

/**
 * @brief Update fireplace flap position display
 * @param value Fireplace flap position (0-100%)
 */
void gui_set_fireplace(uint8_t value);

/**
 * @brief Update ventilation flap position display
 * @param value Ventilation flap position (0-100%)
 */
void gui_set_ventilation(uint8_t value);

/**
 * @brief Update air quality (PPM) display
 * @param value Air quality value in PPM
 */
void gui_set_ppm(uint8_t value);

/**
 * @brief Update fireplace temperature display
 * @param value Temperature in degrees Celsius
 */
void gui_set_fireplace_temperature(uint8_t value);

/**
 * @brief Get pointer to screen buffer for display output
 * @return Pointer to screen buffer string (rows * columns bytes)
 */
char * gui_get_screen_buffer(void);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_GUI_GUI_H_ */
