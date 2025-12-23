/***********************************************************************************************************************
 *
 *            File: ui.c
 *      Created on: Dec 23, 2025
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved.
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "ui.h"
#include "stm32h5xx_hal.h"

/**************************************           DEFINES                    ******************************************/
#define UI_DEBOUNCE_TIME_MS 50
#define UI_LONG_PRESS_TIME_MS 1000

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static void handle_button_press(ui_data_t * this);
static void handle_button_release(ui_data_t * this);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void ui_init(ui_data_t * this,
		void (*shift_focus_left)(void),
		void (*shift_focus_right)(void),
		void (*increase_time)(void),
		void (*decrease_time)(void),
		void (*time_edit_mode)(bool enter))
{
	// Register callbacks
	this->shift_focus_left = shift_focus_left;
	this->shift_focus_right = shift_focus_right;
	this->increase_time = increase_time;
	this->decrease_time = decrease_time;
	this->time_edit_mode = time_edit_mode;

	// Initialize button states
	this->input_up = false;
	this->input_down = false;
	this->input_left = false;
	this->input_right = false;
	this->input_ok = false;

	this->input_up_prev = false;
	this->input_down_prev = false;
	this->input_left_prev = false;
	this->input_right_prev = false;
	this->input_ok_prev = false;

	// Initialize timing
	this->up_press_start_time = 0;
	this->down_press_start_time = 0;
	this->left_press_start_time = 0;
	this->right_press_start_time = 0;
	this->ok_press_start_time = 0;

	// Initialize state flags
	this->up_pressed = false;
	this->down_pressed = false;
	this->left_pressed = false;
	this->right_pressed = false;
	this->ok_pressed = false;
	this->ok_long_press_triggered = false;
}

void ui_main_function(ui_data_t * this)
{
	handle_button_press(this);
	handle_button_release(this);

	// Update previous state
	this->input_up_prev = this->input_up;
	this->input_down_prev = this->input_down;
	this->input_left_prev = this->input_left;
	this->input_right_prev = this->input_right;
	this->input_ok_prev = this->input_ok;
}

void ui_set_input_up(ui_data_t * this, bool val)
{
	this->input_up = val;
}

void ui_set_input_down(ui_data_t * this, bool val)
{
	this->input_down = val;
}

void ui_set_input_left(ui_data_t * this, bool val)
{
	this->input_left = val;
}

void ui_set_input_right(ui_data_t * this, bool val)
{
	this->input_right = val;
}

void ui_set_input_ok(ui_data_t * this, bool val)
{
	this->input_ok = val;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
static void handle_button_press(ui_data_t * this)
{
	uint32_t current_time = HAL_GetTick();

	// Up button
	if (this->input_up && !this->input_up_prev && !this->up_pressed)
	{
		this->up_pressed = true;
		this->up_press_start_time = current_time;
	}

	// Down button
	if (this->input_down && !this->input_down_prev && !this->down_pressed)
	{
		this->down_pressed = true;
		this->down_press_start_time = current_time;
	}

	// Left button
	if (this->input_left && !this->input_left_prev && !this->left_pressed)
	{
		this->left_pressed = true;
		this->left_press_start_time = current_time;
	}

	// Right button
	if (this->input_right && !this->input_right_prev && !this->right_pressed)
	{
		this->right_pressed = true;
		this->right_press_start_time = current_time;
	}

	// OK button
	if (this->input_ok && !this->input_ok_prev && !this->ok_pressed)
	{
		this->ok_pressed = true;
		this->ok_press_start_time = current_time;
		this->ok_long_press_triggered = false;
	}

	// Check for long press of OK button
	if (this->ok_pressed && !this->ok_long_press_triggered)
	{
		if ((current_time - this->ok_press_start_time) >= UI_LONG_PRESS_TIME_MS)
		{
			this->ok_long_press_triggered = true;
			if (this->time_edit_mode != NULL)
			{
				this->time_edit_mode(true);
			}
		}
	}
}

static void handle_button_release(ui_data_t * this)
{
	uint32_t current_time = HAL_GetTick();

	// Up button release
	if (this->up_pressed && !this->input_up)
	{
		uint32_t press_duration = current_time - this->up_press_start_time;
		if (press_duration >= UI_DEBOUNCE_TIME_MS)
		{
			if (this->increase_time != NULL)
			{
				this->increase_time();
			}
		}
		this->up_pressed = false;
	}

	// Down button release
	if (this->down_pressed && !this->input_down)
	{
		uint32_t press_duration = current_time - this->down_press_start_time;
		if (press_duration >= UI_DEBOUNCE_TIME_MS)
		{
			if (this->decrease_time != NULL)
			{
				this->decrease_time();
			}
		}
		this->down_pressed = false;
	}

	// Left button release
	if (this->left_pressed && !this->input_left)
	{
		uint32_t press_duration = current_time - this->left_press_start_time;
		if (press_duration >= UI_DEBOUNCE_TIME_MS)
		{
			if (this->shift_focus_left != NULL)
			{
				this->shift_focus_left();
			}
		}
		this->left_pressed = false;
	}

	// Right button release
	if (this->right_pressed && !this->input_right)
	{
		uint32_t press_duration = current_time - this->right_press_start_time;
		if (press_duration >= UI_DEBOUNCE_TIME_MS)
		{
			if (this->shift_focus_right != NULL)
			{
				this->shift_focus_right();
			}
		}
		this->right_pressed = false;
	}

	// OK button release
	if (this->ok_pressed && !this->input_ok)
	{
		uint32_t press_duration = current_time - this->ok_press_start_time;
		// Only trigger short press callback if long press wasn't triggered
		if (press_duration >= UI_DEBOUNCE_TIME_MS && !this->ok_long_press_triggered)
		{
			if (this->time_edit_mode != NULL)
			{
				this->time_edit_mode(false);
			}
		}
		this->ok_pressed = false;
		this->ok_long_press_triggered = false;
	}
}
