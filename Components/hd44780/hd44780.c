/***********************************************************************************************************************
 *
 *            File: hd44780.c
 *      Created on: Dec 21, 2025
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "hd44780.h"
#include <stddef.h>

/**************************************           DEFINES                    ******************************************/
// HD44780 Commands
#define HD44780_CMD_CLEAR_DISPLAY       0x01
#define HD44780_CMD_RETURN_HOME         0x02
#define HD44780_CMD_ENTRY_MODE_SET      0x04
#define HD44780_CMD_DISPLAY_CONTROL     0x08
#define HD44780_CMD_CURSOR_SHIFT        0x10
#define HD44780_CMD_FUNCTION_SET        0x20
#define HD44780_CMD_SET_CGRAM_ADDR      0x40
#define HD44780_CMD_SET_DDRAM_ADDR      0x80

// HD44780 Timing requirements (in milliseconds)
#define HD44780_DELAY_INIT              15    // Initial delay after power-on
#define HD44780_DELAY_LONG_CMD          2     // Clear display, Return home
#define HD44780_DELAY_SHORT_CMD         1     // All other commands and data

// Entry Mode flags
#define HD44780_ENTRY_SHIFT_INCREMENT   0x01
#define HD44780_ENTRY_SHIFT_DECREMENT   0x00
#define HD44780_ENTRY_DISPLAY_SHIFT     0x01
#define HD44780_ENTRY_DISPLAY_NO_SHIFT  0x00

// Display Control flags
#define HD44780_DISPLAY_ON              0x04
#define HD44780_DISPLAY_OFF             0x00
#define HD44780_CURSOR_ON               0x02
#define HD44780_CURSOR_OFF              0x00
#define HD44780_BLINK_ON                0x01
#define HD44780_BLINK_OFF               0x00

// Function Set flags
#define HD44780_8BIT_MODE               0x10
#define HD44780_4BIT_MODE               0x00
#define HD44780_2LINE                   0x08
#define HD44780_1LINE                   0x00
#define HD44780_5x10_DOTS               0x04
#define HD44780_5x8_DOTS                0x00

// DDRAM addresses for rows
#define HD44780_ROW1_ADDR               0x00
#define HD44780_ROW2_ADDR               0x40
#define HD44780_ROW3_ADDR               0x14
#define HD44780_ROW4_ADDR               0x54

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void hd44780_init(hd44780_data_t * this,
		uint8_t rows,
		uint8_t columns,
		void (* update_pins)(uint8_t d4_d7, bool rs, bool e))
{
	this->rows = rows;
	this->columns = columns;
	this->update_pins = update_pins;
	this->tick_timer = 0;
	this->last_command_time = 0;
	this->init_in_progress = false;
	this->init_step = 0;
	this->state = HD44780_STATE_UNINIT;
	this->transfer_state = HD44780_TRANSFER_IDLE;
	this->current_byte = 0;
	this->current_rs = false;
	this->single_nibble_value = 0;
	this->buffer_write_in_progress = false;
	this->buffer_write_index = 0;
	this->buffer_write_total = 0;
	this->buffer_ptr = NULL;
}

void hd44780_main(hd44780_data_t * this, uint32_t call_period_ms)
{
	// Update tick timer
	this->tick_timer += call_period_ms;

	// ========== UNINIT STATE: Wait for power-on delay ==========
	if (this->state == HD44780_STATE_UNINIT)
	{
		if (this->tick_timer < HD44780_DELAY_INIT)
		{
			return;  // Still waiting for power-on delay
		}
		// Power-on delay complete, move to IDLE and start init
		this->state = HD44780_STATE_IDLE;
		this->init_in_progress = true;
		this->init_step = 0;
		return;
	}

	// ========== TRANSFER STATE: Execute transfer state machine ==========
	if (this->state == HD44780_STATE_TRANSFER)
	{
		if (this->transfer_state == HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH)
		{
			// Send high nibble with E=1
			uint8_t nibble = (this->current_byte >> 4) & 0x0F;
			this->update_pins(nibble, this->current_rs, true);
			// Wait for I2C transfer complete callback
			return;
		}
		else if (this->transfer_state == HD44780_TRANSFER_HIGH_NIBBLE_E_LOW)
		{
			// Clear E after high nibble
			uint8_t nibble = (this->current_byte >> 4) & 0x0F;
			this->update_pins(nibble, this->current_rs, false);
			// Wait for I2C transfer complete callback
			return;
		}
		else if (this->transfer_state == HD44780_TRANSFER_LOW_NIBBLE_E_HIGH)
		{
			// Send low nibble with E=1
			uint8_t nibble = this->current_byte & 0x0F;
			this->update_pins(nibble, this->current_rs, true);
			// Wait for I2C transfer complete callback
			return;
		}
		else if (this->transfer_state == HD44780_TRANSFER_LOW_NIBBLE_E_LOW)
		{
			// Clear E after low nibble
			uint8_t nibble = this->current_byte & 0x0F;
			this->update_pins(nibble, this->current_rs, false);
			// Wait for I2C transfer complete callback
			return;
		}
		else if (this->transfer_state == HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH)
		{
			// Send single nibble with E=1 (init only)
			this->update_pins(this->single_nibble_value, false, true);
			// Wait for I2C transfer complete callback
			return;
		}
		else if (this->transfer_state == HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW)
		{
			// Clear E after single nibble
			this->update_pins(this->single_nibble_value, false, false);
			// Wait for I2C transfer complete callback
			return;
		}
	}

	// ========== IDLE STATE: Prepare next command ==========
	if (this->state == HD44780_STATE_IDLE)
	{
		// Check if enough time has passed since last command
		uint32_t time_since_last_cmd = this->tick_timer - this->last_command_time;
		uint32_t required_delay = HD44780_DELAY_SHORT_CMD;

		// Determine required delay based on last command
		if (this->current_byte == HD44780_CMD_CLEAR_DISPLAY ||
		    this->current_byte == HD44780_CMD_RETURN_HOME)
		{
			required_delay = HD44780_DELAY_LONG_CMD;
		}

		// Wait if not enough time has passed
		if (time_since_last_cmd < required_delay)
		{
			return;
		}

		// Process buffer write if in progress
		if (this->buffer_write_in_progress && this->buffer_write_index < this->buffer_write_total)
		{
			uint16_t chars_written = this->buffer_write_index % (this->columns + 1);

			if (chars_written == 0)
			{
				// Send cursor position command for new row
				uint8_t row_offsets[] = {HD44780_ROW1_ADDR, HD44780_ROW2_ADDR,
				                         HD44780_ROW3_ADDR, HD44780_ROW4_ADDR};
				uint8_t current_row = this->buffer_write_index / (this->columns + 1);
				this->current_byte = HD44780_CMD_SET_DDRAM_ADDR | row_offsets[current_row];
				this->current_rs = false;
				this->state = HD44780_STATE_TRANSFER;
				this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH;
				this->last_command_time = this->tick_timer;
			}
			else
			{
				// Send character data
				uint16_t buffer_char_index = (this->buffer_write_index / (this->columns + 1)) *
				                             this->columns + (chars_written - 1);
				this->current_byte = (uint8_t)this->buffer_ptr[buffer_char_index];
				this->current_rs = true;
				this->state = HD44780_STATE_TRANSFER;
				this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH;
				this->last_command_time = this->tick_timer;
			}

			this->buffer_write_index++;

			// Check if we're done
			if (this->buffer_write_index >= this->buffer_write_total)
			{
				this->buffer_write_in_progress = false;
			}

			return;  // Start transfer next cycle
		}

		// Process initialization if in progress
		if (this->init_in_progress)
		{
			switch (this->init_step)
			{
				case 0:
					// First: Function set (8-bit mode) - single nibble
					this->single_nibble_value = 0x03;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					break;

				case 1:
					// Second: Function set (8-bit mode) - single nibble
					this->single_nibble_value = 0x03;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					break;

				case 2:
					// Third: Function set (8-bit mode) - single nibble
					this->single_nibble_value = 0x03;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					break;

				case 3:
					// Fourth: Function set (4-bit mode) - single nibble
					this->single_nibble_value = 0x02;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					break;

				case 4:
					// Function set: 4-bit mode, 2 lines, 5x8 dots
					this->current_byte = HD44780_CMD_FUNCTION_SET | HD44780_4BIT_MODE |
					                     HD44780_2LINE | HD44780_5x8_DOTS;
					this->current_rs = false;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					break;

				case 5:
					// Display control: Display on, cursor off, blink off
					this->current_byte = HD44780_CMD_DISPLAY_CONTROL | HD44780_DISPLAY_ON |
					                     HD44780_CURSOR_OFF | HD44780_BLINK_OFF;
					this->current_rs = false;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					break;

				case 6:
					// Clear display
					this->current_byte = HD44780_CMD_CLEAR_DISPLAY;
					this->current_rs = false;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					break;

				case 7:
					// Entry mode set: Increment cursor, no display shift
					this->current_byte = HD44780_CMD_ENTRY_MODE_SET | HD44780_ENTRY_SHIFT_INCREMENT |
					                     HD44780_ENTRY_DISPLAY_NO_SHIFT;
					this->current_rs = false;
					this->state = HD44780_STATE_TRANSFER;
					this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH;
					this->last_command_time = this->tick_timer;
					this->init_step++;
					this->init_in_progress = false;  // Done!
					break;

				default:
					this->init_in_progress = false;
					break;
			}
		}
	}
}

void hd44780_transfer_complete(hd44780_data_t * this)
{
	// This callback is called when the I2C transfer to PCF8574 completes
	// Advance the transfer state machine to the next step

	if (this->state != HD44780_STATE_TRANSFER)
	{
		return;  // Spurious callback
	}

	// Advance transfer state machine
	switch (this->transfer_state)
	{
		case HD44780_TRANSFER_HIGH_NIBBLE_E_HIGH:
			// Just sent high nibble with E=1, now clear E
			this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
			break;

		case HD44780_TRANSFER_HIGH_NIBBLE_E_LOW:
			// Just cleared E after high nibble, now send low nibble
			this->transfer_state = HD44780_TRANSFER_LOW_NIBBLE_E_HIGH;
			break;

		case HD44780_TRANSFER_LOW_NIBBLE_E_HIGH:
			// Just sent low nibble with E=1, now clear E
			this->transfer_state = HD44780_TRANSFER_LOW_NIBBLE_E_LOW;
			break;

		case HD44780_TRANSFER_LOW_NIBBLE_E_LOW:
			// Transfer complete! Return to IDLE
			this->transfer_state = HD44780_TRANSFER_IDLE;
			this->state = HD44780_STATE_IDLE;
			break;

		case HD44780_TRANSFER_SINGLE_NIBBLE_E_HIGH:
			// Just sent single nibble with E=1, now clear E
			this->transfer_state = HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW;
			break;

		case HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW:
			// Single nibble transfer complete! Return to IDLE
			this->transfer_state = HD44780_TRANSFER_IDLE;
			this->state = HD44780_STATE_IDLE;
			break;

		default:
			// Unexpected state, return to IDLE
			this->transfer_state = HD44780_TRANSFER_IDLE;
			this->state = HD44780_STATE_IDLE;
			break;
	}
}

void hd44780_write_buffer(hd44780_data_t * this, const char * buffer)
{
	if (this->state == HD44780_STATE_IDLE && !this->buffer_write_in_progress)
	{
		this->buffer_ptr = buffer;
		this->buffer_write_index = 0;
		// Total includes: rows * (1 command + columns characters)
		this->buffer_write_total = this->rows * (this->columns + 1);
		this->buffer_write_in_progress = true;
	}
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
