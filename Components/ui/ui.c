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
		void (*short_press_up)(void),
		void (*short_press_down)(void),
		void (*short_press_left)(void),
		void (*short_press_right)(void),
		void (*long_press_left_and_right)(void))
{
	// Register callbacks
	this->short_press_up = short_press_up;
	this->short_press_down = short_press_down;
	this->short_press_left = short_press_left;
	this->short_press_right = short_press_right;
	this->long_press_left_and_right = long_press_left_and_right;

	// Initialize button states
	this->input_up = false;
	this->input_down = false;
	this->input_left = false;
	this->input_right = false;

	this->input_up_prev = false;
	this->input_down_prev = false;
	this->input_left_prev = false;
	this->input_right_prev = false;

	// Initialize timing
	this->up_press_start_time = 0;
	this->down_press_start_time = 0;
	this->left_press_start_time = 0;
	this->right_press_start_time = 0;
	this->left_right_press_start_time = 0;

	// Initialize state flags
	this->up_pressed = false;
	this->down_pressed = false;
	this->left_pressed = false;
	this->right_pressed = false;
	this->left_and_right_pressed = false;
	this->long_press_triggered = false;
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

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
static void handle_button_press(ui_data_t * this)
{
	uint32_t current_time = HAL_GetTick();

	// Detect left AND right pressed together (for long press detection)
	if (this->input_left && this->input_right && !this->left_and_right_pressed)
	{
		this->left_and_right_pressed = true;
		this->left_right_press_start_time = current_time;
		this->long_press_triggered = false;
		// Clear individual press flags to prevent short press callbacks on release
		this->left_pressed = false;
		this->right_pressed = false;
	}

	// Check for long press of left and right together
	if (this->left_and_right_pressed && !this->long_press_triggered)
	{
		if ((current_time - this->left_right_press_start_time) >= UI_LONG_PRESS_TIME_MS)
		{
			this->long_press_triggered = true;
			if (this->long_press_left_and_right != NULL)
			{
				this->long_press_left_and_right();
			}
		}
	}

	// Detect individual button presses (only if not in combined press mode)
	if (!this->left_and_right_pressed)
	{
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
	}
}

static void handle_button_release(ui_data_t * this)
{
	uint32_t current_time = HAL_GetTick();

	// Handle left and right release
	if (this->left_and_right_pressed && (!this->input_left || !this->input_right))
	{
		// Clear individual press flags to prevent short press callbacks
		this->left_pressed = false;
		this->right_pressed = false;
		this->left_and_right_pressed = false;
		this->long_press_triggered = false;
	}

	// Handle individual button releases (only trigger callback if it was a short press)
	// Up button release
	if (this->up_pressed && !this->input_up)
	{
		uint32_t press_duration = current_time - this->up_press_start_time;
		if (press_duration >= UI_DEBOUNCE_TIME_MS)
		{
			if (this->short_press_up != NULL)
			{
				this->short_press_up();
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
			if (this->short_press_down != NULL)
			{
				this->short_press_down();
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
			if (this->short_press_left != NULL)
			{
				this->short_press_left();
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
			if (this->short_press_right != NULL)
			{
				this->short_press_right();
			}
		}
		this->right_pressed = false;
	}
}
