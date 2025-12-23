/***********************************************************************************************************************
 *
 *            File: gui.c
 *      Created on: Dec 22, 2025
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "gui.h"
#include "ds3231.h"
#include "stdint.h"
#include "stm32h5xx_hal.h"

/**************************************           DEFINES                    ******************************************/
// Screen buffer positions
#define FIREPLACE_POS       4   // K%: XXX (position for XXX)
#define TEMP_POS           17   // Kt: XXX (position for XXX)
#define VENTILATION_POS    24   // W%: XXX (position for XXX)
#define PPM_POS            37   // Wppm: XXX (position for XXX)
#define TIME_POS           60   // HH:MM:SS (position for HH)
#define DATE_POS           72   // DD.MM.YY (position for DD)

// Blink timing (configurable)
#define GUI_BLINK_ON_TIME_MS   800
#define GUI_BLINK_OFF_TIME_MS  800

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
/*
"K%: XXX        Kt: XXX"
"W%: XXX      Wppm: XXX"
"                    "
"22:33:44    12.03.25";
*/

static gui_focus_t current_focus = GUI_FOCUS_NONE;
static bool blink_state = true;  // true = visible, false = hidden
static uint32_t last_blink_toggle_time = 0;
static char focused_field_backup[2] = {' ', ' '};  // Backup of the 2 digits being blinked

char screen_buffer[81] =  // 80 chars + null terminator for safe snprintf
{
	// Line 1: "K%:         Kt:    "
	'K', '%', ':', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
	'K', 't', ':', ' ', ' ', ' ', ' ',
	// Line 2: "W%:       Wppm:    "
	'W', '%', ':', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
	'W', 'p', 'p', 'm', ':', ' ', ' ', ' ', ' ',
	// Line 3: "                    "
	' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
	' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
	// Line 4: "  :  :        .  .  "
	' ', ' ', ':', ' ', ' ', ':', ' ', ' ', ' ', ' ',
	' ', ' ', ' ', ' ', '.', ' ', ' ', '.', ' ', ' ',
	'\0'  // Null terminator
};

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static void format_2digit(char *buffer, uint8_t value);
static void format_3digit_right_justified(char *buffer, uint8_t value);
static uint8_t get_focus_position(gui_focus_t focus);
static void apply_blink_state(void);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void gui_main(void)
{
	if (current_focus == GUI_FOCUS_NONE)
	{
		return;
	}

	uint32_t current_time = HAL_GetTick();
	uint32_t blink_period = blink_state ? GUI_BLINK_ON_TIME_MS : GUI_BLINK_OFF_TIME_MS;

	// Check if it's time to toggle blink state
	if ((current_time - last_blink_toggle_time) >= blink_period)
	{
		// Toggle blink state
		blink_state = !blink_state;
		last_blink_toggle_time = current_time;

		// Apply the new blink state to screen buffer
		apply_blink_state();
	}
}

void gui_focus(gui_focus_t focus)
{
	// Restore previous focused field if it was hidden
	if (current_focus != GUI_FOCUS_NONE && !blink_state)
	{
		uint8_t pos = get_focus_position(current_focus);
		screen_buffer[pos] = focused_field_backup[0];
		screen_buffer[pos + 1] = focused_field_backup[1];
	}

	// Update focus
	current_focus = focus;
	blink_state = true;
	last_blink_toggle_time = HAL_GetTick();

	// Backup the new focused field
	if (current_focus != GUI_FOCUS_NONE)
	{
		uint8_t pos = get_focus_position(current_focus);
		focused_field_backup[0] = screen_buffer[pos];
		focused_field_backup[1] = screen_buffer[pos + 1];
	}
}

void gui_set_time(time_data_t * time)
{
	// time_data_t already contains decimal values (converted by ds3231 driver)
	// Clamp values to valid ranges
	uint8_t hour = (time->hour > 23) ? 23 : time->hour;
	uint8_t minute = (time->minute > 59) ? 59 : time->minute;
	uint8_t second = (time->second > 59) ? 59 : time->second;
	uint8_t day = (time->day_of_month > 31) ? 31 : time->day_of_month;
	uint8_t month = (time->month > 12) ? 12 : time->month;
	uint8_t year = (time->year > 99) ? 99 : time->year;

	// Format time as HH:MM:SS at position TIME_POS (no null terminator)
	// Skip updating the focused field if it's currently hidden (blinking off)
	if (current_focus != GUI_FOCUS_HOUR || blink_state)
	{
		format_2digit(&screen_buffer[TIME_POS], hour);
	}

	if (current_focus != GUI_FOCUS_MINUTE || blink_state)
	{
		format_2digit(&screen_buffer[TIME_POS + 3], minute);
	}

	if (current_focus != GUI_FOCUS_SECOND || blink_state)
	{
		format_2digit(&screen_buffer[TIME_POS + 6], second);
	}

	// Format date as DD.MM.YY at position DATE_POS (no null terminator)
	if (current_focus != GUI_FOCUS_DAY || blink_state)
	{
		format_2digit(&screen_buffer[DATE_POS], day);
	}

	if (current_focus != GUI_FOCUS_MONTH || blink_state)
	{
		format_2digit(&screen_buffer[DATE_POS + 3], month);
	}

	if (current_focus != GUI_FOCUS_YEAR || blink_state)
	{
		format_2digit(&screen_buffer[DATE_POS + 6], year);
	}

	// Update backup if we just modified a focused field (and it's visible)
	if (current_focus != GUI_FOCUS_NONE && blink_state)
	{
		uint8_t pos = get_focus_position(current_focus);
		focused_field_backup[0] = screen_buffer[pos];
		focused_field_backup[1] = screen_buffer[pos + 1];
	}
}

void gui_set_fireplace(uint8_t value)
{
	// Update K%: XXX at position FIREPLACE_POS (right justified in 3 chars)
	format_3digit_right_justified(&screen_buffer[FIREPLACE_POS], value);
}

void gui_set_ventilation(uint8_t value)
{
	// Update W%: XXX at position VENTILATION_POS (right justified in 3 chars)
	format_3digit_right_justified(&screen_buffer[VENTILATION_POS], value);
}

void gui_set_ppm(uint8_t value)
{
	// Update Wppm: XXX at position PPM_POS (right justified in 3 chars)
	format_3digit_right_justified(&screen_buffer[PPM_POS], value);
}

void gui_set_fireplace_temperature(uint8_t value)
{
	// Update Kt: XXX at position TEMP_POS (right justified in 3 chars)
	format_3digit_right_justified(&screen_buffer[TEMP_POS], value);
}

char * gui_get_screen_buffer(void)
{
	return screen_buffer;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
static uint8_t get_focus_position(gui_focus_t focus)
{
	switch (focus)
	{
		case GUI_FOCUS_HOUR:
			return TIME_POS;
		case GUI_FOCUS_MINUTE:
			return TIME_POS + 3;
		case GUI_FOCUS_SECOND:
			return TIME_POS + 6;
		case GUI_FOCUS_DAY:
			return DATE_POS;
		case GUI_FOCUS_MONTH:
			return DATE_POS + 3;
		case GUI_FOCUS_YEAR:
			return DATE_POS + 6;
		case GUI_FOCUS_NONE:
		default:
			return 0;
	}
}

static void apply_blink_state(void)
{
	if (current_focus == GUI_FOCUS_NONE)
	{
		return;
	}

	uint8_t pos = get_focus_position(current_focus);

	if (blink_state)
	{
		// Visible - restore from backup
		screen_buffer[pos] = focused_field_backup[0];
		screen_buffer[pos + 1] = focused_field_backup[1];
	}
	else
	{
		// Hidden - replace with spaces
		screen_buffer[pos] = ' ';
		screen_buffer[pos + 1] = ' ';
	}
}

static void format_2digit(char *buffer, uint8_t value)
{
	// Format value as 2 digits with leading zero (no null terminator)
	buffer[0] = '0' + (value / 10);
	buffer[1] = '0' + (value % 10);
}

static void format_3digit_right_justified(char *buffer, uint8_t value)
{
	// Format value as 3 characters, right justified with spaces (no null terminator)
	if (value >= 100)
	{
		buffer[0] = '0' + (value / 100);
		buffer[1] = '0' + ((value / 10) % 10);
		buffer[2] = '0' + (value % 10);
	}
	else if (value >= 10)
	{
		buffer[0] = ' ';
		buffer[1] = '0' + (value / 10);
		buffer[2] = '0' + (value % 10);
	}
	else
	{
		buffer[0] = ' ';
		buffer[1] = ' ';
		buffer[2] = '0' + value;
	}
}
