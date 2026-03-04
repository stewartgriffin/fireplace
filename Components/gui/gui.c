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
#define TEMP_POS           17   // Kom t: XXX (position for XXX)
#define VENTILATION_POS    24   // W%: XXX (position for XXX)
#define PPM_POS            37   // Zew t: XXX (position for XXX)
#define TIME_POS           60   // HH:MM:SS (position for HH)
#define DATE_POS           72   // DD.MM.YY (position for DD)

// Blink timing (configurable)
#define GUI_BLINK_ON_TIME_MS   500
#define GUI_BLINK_OFF_TIME_MS  300

/**************************************           DATA TYPES                 ******************************************/
typedef enum
{
	GUI_UPDATE_IDLE,     // Ready to scan for changes
	GUI_UPDATE_SCANNING, // Currently scanning for next changed region
	GUI_UPDATE_SENDING   // Attempting to send a changed region
} gui_update_state_t;

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/
/*
"K%: XXX     Kom t: XXX"
"W%: XXX     Zew t: XXX"
"                    "
"22:33:44    12.03.25";
*/

static gui_focus_t current_focus = GUI_FOCUS_NONE;
static bool blink_state = true;  // true = visible, false = hidden
static uint32_t last_blink_toggle_time = 0;
static char focused_field_backup[2] = {' ', ' '};  // Backup of the 2 digits being blinked

// Display update tracking
static char display_buffer[80];  // What's currently shown on LCD (no null terminator)
static int (*update_partial_screen_callback)(const char *buffer, uint16_t position, uint16_t length) = NULL;
static gui_update_state_t update_state = GUI_UPDATE_IDLE;
static uint16_t scan_position = 0;
static uint16_t change_start = 0;
static uint16_t change_length = 0;
static char change_buffer[2][80];  // Double buffer for changed regions (prevents corruption while HD44780 reads)
static uint8_t active_change_buffer = 0;  // Which buffer is currently being filled (0 or 1)

char screen_buffer[81] =  // 80 chars + null terminator for safe snprintf
{
	// Line 1: "K%:       Kom t:   "
	'K', '%', ':', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'K', 'o', 'm',
	' ', 't', ':', ' ', ' ', ' ', ' ',
	// Line 2: "W%:       Zew t:   "
	'W', '%', ':', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'Z',
	'e', 'w', ' ', 't', ':', ' ', ' ', ' ', ' ',
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
static void gui_focus(gui_focus_t focus);
static void process_display_updates(void);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void gui_init(int (*update_partial_screen)(const char *buffer, uint16_t position, uint16_t length))
{
	update_partial_screen_callback = update_partial_screen;
	update_state = GUI_UPDATE_IDLE;
	scan_position = 0;

	// Initialize display_buffer to all spaces (blank LCD)
	// This ensures all initial content (including static labels) gets sent
	for (uint16_t i = 0; i < 80; i++)
	{
		display_buffer[i] = ' ';
	}
}

void gui_main(void)
{
	// Handle blinking
	if (current_focus != GUI_FOCUS_NONE)
	{
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

	// Process display updates (compare buffers and send changes)
	if (update_partial_screen_callback != NULL)
	{
		process_display_updates();
	}
}

void gui_shift_focus_left(void)
{
	if (current_focus == GUI_FOCUS_NONE)
	{
		return;
	}

	// Cycle through all time/date fields left: YEAR -> MONTH -> DAY -> SECOND -> MINUTE -> HOUR
	gui_focus_t new_focus;
	switch (current_focus)
	{
		case GUI_FOCUS_YEAR:
			new_focus = GUI_FOCUS_MONTH;
			break;
		case GUI_FOCUS_MONTH:
			new_focus = GUI_FOCUS_DAY;
			break;
		case GUI_FOCUS_DAY:
			new_focus = GUI_FOCUS_SECOND;
			break;
		case GUI_FOCUS_SECOND:
			new_focus = GUI_FOCUS_MINUTE;
			break;
		case GUI_FOCUS_MINUTE:
			new_focus = GUI_FOCUS_HOUR;
			break;
		case GUI_FOCUS_HOUR:
			new_focus = GUI_FOCUS_YEAR;
			break;
		default:
			new_focus = GUI_FOCUS_HOUR;
			break;
	}

	gui_focus(new_focus);
}

void gui_shift_focus_right(void)
{
	if (current_focus == GUI_FOCUS_NONE)
	{
		return;
	}

	// Cycle through all time/date fields right: HOUR -> MINUTE -> SECOND -> DAY -> MONTH -> YEAR
	gui_focus_t new_focus;
	switch (current_focus)
	{
		case GUI_FOCUS_HOUR:
			new_focus = GUI_FOCUS_MINUTE;
			break;
		case GUI_FOCUS_MINUTE:
			new_focus = GUI_FOCUS_SECOND;
			break;
		case GUI_FOCUS_SECOND:
			new_focus = GUI_FOCUS_DAY;
			break;
		case GUI_FOCUS_DAY:
			new_focus = GUI_FOCUS_MONTH;
			break;
		case GUI_FOCUS_MONTH:
			new_focus = GUI_FOCUS_YEAR;
			break;
		case GUI_FOCUS_YEAR:
			new_focus = GUI_FOCUS_HOUR;
			break;
		default:
			new_focus = GUI_FOCUS_HOUR;
			break;
	}

	gui_focus(new_focus);
}

void gui_time_edit_mode(bool enable)
{
	if (enable)
	{
		// Enter time edit mode - focus on hour
		gui_focus(GUI_FOCUS_HOUR);
	}
	else
	{
		// Exit time edit mode - remove focus
		gui_focus(GUI_FOCUS_NONE);
	}
}

gui_focus_t gui_get_focus(void)
{
	return current_focus;
}

static void gui_focus(gui_focus_t focus)
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
	// Update Zew t: XXX at position PPM_POS (right justified in 3 chars)
	format_3digit_right_justified(&screen_buffer[PPM_POS], value);
}

void gui_set_fireplace_temperature(uint8_t value)
{
	// Update Kom t: XXX at position TEMP_POS (right justified in 3 chars)
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

static void process_display_updates(void)
{
	switch (update_state)
	{
		case GUI_UPDATE_IDLE:
			// Start scanning from beginning
			scan_position = 0;
			update_state = GUI_UPDATE_SCANNING;
			// Fall through to start scanning immediately
			__attribute__((fallthrough));

		case GUI_UPDATE_SCANNING:
		{
			// Scan for next continuous changed region
			bool found_change = false;
			change_start = 0;
			change_length = 0;

			// Find start of next changed region
			while (scan_position < 80)
			{
				if (screen_buffer[scan_position] != display_buffer[scan_position])
				{
					found_change = true;
					change_start = scan_position;
					break;
				}
				scan_position++;
			}

			if (!found_change)
			{
				// No more changes found, return to IDLE
				update_state = GUI_UPDATE_IDLE;
				return;
			}

			// Found start of change, now find the length of continuous changed region
			change_length = 0;
			while (scan_position < 80 &&
			       screen_buffer[scan_position] != display_buffer[scan_position])
			{
				// Copy to the currently active change buffer
				change_buffer[active_change_buffer][change_length] = screen_buffer[scan_position];
				change_length++;
				scan_position++;
			}

			// Move to SENDING state
			update_state = GUI_UPDATE_SENDING;
			// Fall through to send immediately
			__attribute__((fallthrough));
		}

		case GUI_UPDATE_SENDING:
		{
			// Try to send the changed region (from the active buffer)
			// Get pointer to the active buffer
			char *send_buffer = &change_buffer[active_change_buffer][0];
			int result = update_partial_screen_callback(send_buffer, change_start, change_length);

			if (result == 0)
			{
				// Success - update display_buffer with sent data
				for (uint16_t i = 0; i < change_length; i++)
				{
					display_buffer[change_start + i] = change_buffer[active_change_buffer][i];
				}

				// Switch to the other buffer (HD44780 will keep reading from the old one)
				active_change_buffer = 1 - active_change_buffer;

				// Continue scanning for more changes
				update_state = GUI_UPDATE_SCANNING;
			}
			else
			{
				// Display is busy, stay in SENDING state and try again later
				// Don't advance scan_position, we'll retry the same region
			}
			break;
		}
	}
}
