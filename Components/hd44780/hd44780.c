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
	this->init_in_progress = true;
	this->init_step = 0;
	this->state = HD44780_STATE_IDLE;
	this->current_byte = 0;
	this->current_rs = false;
	this->high_nibble_sent = false;
	this->buffer_write_in_progress = false;
	this->buffer_write_index = 0;
	this->buffer_write_total = 0;
	this->buffer_ptr = NULL;
}

void hd44780_main(hd44780_data_t * this, uint32_t call_period_ms)
{
	// Handle state machine for byte transmission
	if (this->state == HD44780_STATE_SEND_HIGH_NIBBLE)
	{
		// Send high nibble with E=1 to latch data
		this->update_pins((this->current_byte >> 4) & 0x0F, this->current_rs, true);
		this->state = HD44780_STATE_WAIT_TRANSFER;
	}
	else if (this->state == HD44780_STATE_SEND_LOW_NIBBLE)
	{
		// Send low nibble with E=1 to latch data
		this->update_pins(this->current_byte & 0x0F, this->current_rs, true);
		this->state = HD44780_STATE_WAIT_TRANSFER;
	}
	else if (this->state == HD44780_STATE_IDLE)
	{
		// Process buffer write if in progress
		if (this->buffer_write_in_progress == true && this->buffer_write_index < this->buffer_write_total)
		{
			// Determine if this is a row change command or data
			uint16_t chars_written = this->buffer_write_index % (this->columns + 1);

			if (chars_written == 0)
			{
				// Send cursor position command for new row
				uint8_t row_offsets[] = {HD44780_ROW1_ADDR, HD44780_ROW2_ADDR, HD44780_ROW3_ADDR, HD44780_ROW4_ADDR};
				uint8_t current_row = this->buffer_write_index / (this->columns + 1);
				this->current_byte = HD44780_CMD_SET_DDRAM_ADDR | row_offsets[current_row];
				this->current_rs = false;
				this->state = HD44780_STATE_SEND_HIGH_NIBBLE;
			}
			else
			{
				// Send character data
				uint16_t buffer_char_index = (this->buffer_write_index / (this->columns + 1)) * this->columns + (chars_written - 1);
				this->current_byte = (uint8_t)this->buffer_ptr[buffer_char_index];
				this->current_rs = true;
				this->state = HD44780_STATE_SEND_HIGH_NIBBLE;
			}

			this->buffer_write_index++;

			// Check if we're done
			if (this->buffer_write_index >= this->buffer_write_total)
			{
				this->buffer_write_in_progress = false;
			}
		}
		// Only process initialization when state machine is idle
		else if (this->init_in_progress == true)
		{
			// HD44780 initialization sequence (4-bit mode)
			switch (this->init_step)
			{
				case 0:
					// First: Function set (8-bit mode) - single nibble
					this->current_byte = 0xFF;  // Special marker for single nibble
					this->current_rs = false;
					this->update_pins(0x03, false, true);
					this->state = HD44780_STATE_WAIT_TRANSFER;
					this->init_step++;
					break;

				case 1:
					// Second: Function set (8-bit mode) - single nibble
					this->current_byte = 0xFF;  // Special marker for single nibble
					this->current_rs = false;
					this->update_pins(0x03, false, true);
					this->state = HD44780_STATE_WAIT_TRANSFER;
					this->init_step++;
					break;

				case 2:
					// Third: Function set (8-bit mode) - single nibble
					this->current_byte = 0xFF;  // Special marker for single nibble
					this->current_rs = false;
					this->update_pins(0x03, false, true);
					this->state = HD44780_STATE_WAIT_TRANSFER;
					this->init_step++;
					break;

				case 3:
					// Fourth: Function set (4-bit mode) - single nibble
					this->current_byte = 0xFF;  // Special marker for single nibble
					this->current_rs = false;
					this->update_pins(0x02, false, true);
					this->state = HD44780_STATE_WAIT_TRANSFER;
					this->init_step++;
					break;

				case 4:
					// Function set: 4-bit mode, 2 lines, 5x8 dots
					this->current_byte = HD44780_CMD_FUNCTION_SET | HD44780_4BIT_MODE | HD44780_2LINE | HD44780_5x8_DOTS;
					this->current_rs = false;
					this->state = HD44780_STATE_SEND_HIGH_NIBBLE;
					this->init_step++;
					break;

				case 5:
					// Display control: Display on, cursor off, blink off
					this->current_byte = HD44780_CMD_DISPLAY_CONTROL | HD44780_DISPLAY_ON | HD44780_CURSOR_OFF | HD44780_BLINK_OFF;
					this->current_rs = false;
					this->state = HD44780_STATE_SEND_HIGH_NIBBLE;
					this->init_step++;
					break;

				case 6:
					// Clear display
					this->current_byte = HD44780_CMD_CLEAR_DISPLAY;
					this->current_rs = false;
					this->state = HD44780_STATE_SEND_HIGH_NIBBLE;
					this->init_step++;
					break;

				case 7:
					// Entry mode set: Increment cursor, no display shift
					this->current_byte = HD44780_CMD_ENTRY_MODE_SET | HD44780_ENTRY_SHIFT_INCREMENT | HD44780_ENTRY_DISPLAY_NO_SHIFT;
					this->current_rs = false;
					this->state = HD44780_STATE_SEND_HIGH_NIBBLE;
					this->init_step++;
					this->init_in_progress = false;
					break;

				default:
					this->init_in_progress = false;
					break;
			}
		}
	}

	// Increment tick timer
	this->tick_timer += call_period_ms;
}

void hd44780_transfer_complete(hd44780_data_t * this)
{
	if (this->state == HD44780_STATE_WAIT_TRANSFER)
	{
		// Check if we just sent a single nibble (4-bit command during init)
		if (this->current_byte == 0xFF)
		{
			// Single nibble command complete
			this->state = HD44780_STATE_IDLE;
		}
		else
		{
			// We sent a nibble as part of a full byte transmission
			if (!this->high_nibble_sent)
			{
				// Just finished sending high nibble, now send low nibble
				this->high_nibble_sent = true;
				this->state = HD44780_STATE_SEND_LOW_NIBBLE;
			}
			else
			{
				// Just finished sending low nibble, byte transmission complete
				this->high_nibble_sent = false;
				this->state = HD44780_STATE_IDLE;
			}
		}
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
