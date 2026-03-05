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
#include "stm32h5xx_hal.h"
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
#define HD44780_DELAY_SHORT_CMD         0     // All other commands (I2C transfer time provides sufficient delay)

// Entry Mode flags
#define HD44780_ENTRY_INCREMENT         0x02  // I/D bit: 1=increment, 0=decrement
#define HD44780_ENTRY_DECREMENT         0x00
#define HD44780_ENTRY_SHIFT_ON          0x01  // S bit: 1=shift display, 0=cursor moves
#define HD44780_ENTRY_SHIFT_OFF         0x00

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
	this->last_tick = HAL_GetTick();
	this->delay = 0;
	this->init_step = 0;
	this->transfer_complete = true;  // Set true initially so UNINIT state can run
	this->state = HD44780_STATE_UNINIT;
	this->transfer_state = HD44780_TRANSFER_IDLE;
	this->current_byte = 0;
	this->current_rs = false;
	this->single_nibble_value = 0;
	this->buffer_write_in_progress = false;
	this->buffer_write_index = 0;
	this->buffer_write_total = 0;
	this->buffer_ptr = NULL;
	this->buffer_write_partial = false;
	this->buffer_write_start_pos = 0;
	this->buffer_write_length = 0;
	this->buffer_write_chars_sent = 0;
	this->buffer_write_need_cursor = false;
}

void hd44780_main(hd44780_data_t * this)
{
	// Calculate elapsed time since last call
	uint32_t current_tick = HAL_GetTick();
	uint32_t elapsed_ms = current_tick - this->last_tick;
	this->last_tick = current_tick;

	// Update tick timer
	this->tick_timer += elapsed_ms;

	// Wait for I2C transfer to complete
	if (!this->transfer_complete)
	{
		return;
	}

	// Decrement delay timer
	if (this->delay > elapsed_ms)
	{
		this->delay -= elapsed_ms;
	}
	else
	{
		this->delay = 0;
	}

	// Wait for delay before processing
	if (this->delay > 0)
	{
		return;
	}

	// ========== STATE MACHINE ==========
	if (this->state == HD44780_STATE_UNINIT)
	{
		// Wait for power-on delay
		if (this->tick_timer >= HD44780_DELAY_INIT)
		{
			this->state = HD44780_STATE_INITIALIZING;
			this->init_step = 0;
			this->transfer_state = HD44780_TRANSFER_IDLE;
		}
	}
	else if (this->state == HD44780_STATE_INITIALIZING)
	{
		switch (this->transfer_state)
		{
			case HD44780_TRANSFER_IDLE:
				// Ready to start a new transfer - process next init step
				switch (this->init_step)
				{
					case 0:
					case 1:
					case 2:
						// First three: Function set (8-bit mode) - single nibble 0x03
						this->single_nibble_value = 0x03;
						this->transfer_complete = false;
						this->update_pins(this->single_nibble_value, false, true);
						this->transfer_state = HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW;
						return;

					case 3:
						// Fourth: Function set (4-bit mode) - single nibble 0x02
						this->single_nibble_value = 0x02;
						this->transfer_complete = false;
						this->update_pins(this->single_nibble_value, false, true);
						this->transfer_state = HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW;
						return;

					case 4:
					{
						// Function set: 4-bit mode, 2 lines, 5x8 dots
						this->current_byte = HD44780_CMD_FUNCTION_SET | HD44780_4BIT_MODE |
						                     HD44780_2LINE | HD44780_5x8_DOTS;
						this->current_rs = false;
						uint8_t nibble = (this->current_byte >> 4) & 0x0F;
						this->transfer_complete = false;
						this->update_pins(nibble, false, true);
						this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
						return;
					}

					case 5:
					{
						// Display control: Display on, cursor off, blink off
						this->current_byte = HD44780_CMD_DISPLAY_CONTROL | HD44780_DISPLAY_ON |
						                     HD44780_CURSOR_OFF | HD44780_BLINK_OFF;
						this->current_rs = false;
						uint8_t nibble = (this->current_byte >> 4) & 0x0F;
						this->transfer_complete = false;
						this->update_pins(nibble, false, true);
						this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
						return;
					}

					case 6:
					{
						// Clear display
						this->current_byte = HD44780_CMD_CLEAR_DISPLAY;
						this->current_rs = false;
						uint8_t nibble = (this->current_byte >> 4) & 0x0F;
						this->transfer_complete = false;
						this->update_pins(nibble, false, true);
						this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
						return;
					}

					case 7:
					{
						// Entry mode set: Increment cursor, no display shift
						this->current_byte = HD44780_CMD_ENTRY_MODE_SET | HD44780_ENTRY_INCREMENT |
						                     HD44780_ENTRY_SHIFT_OFF;
						this->current_rs = false;
						uint8_t nibble = (this->current_byte >> 4) & 0x0F;
						this->transfer_complete = false;
						this->update_pins(nibble, false, true);
						this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
						return;
					}

					case 8:
						// Initialization complete! Move to IDLE
						this->state = HD44780_STATE_IDLE;
						break;

					default:
						this->state = HD44780_STATE_IDLE;
						break;
				}
				break;

			case HD44780_TRANSFER_HIGH_NIBBLE_E_LOW:
				// Lower E for high nibble
				{
					uint8_t nibble = (this->current_byte >> 4) & 0x0F;
					this->transfer_complete = false;
					this->update_pins(nibble, this->current_rs, false);
					this->transfer_state = HD44780_TRANSFER_LOW_NIBBLE_E_HIGH;
					return;
				}

			case HD44780_TRANSFER_LOW_NIBBLE_E_HIGH:
				// Raise E for low nibble
				{
					uint8_t nibble = this->current_byte & 0x0F;
					this->transfer_complete = false;
					this->update_pins(nibble, this->current_rs, true);
					this->transfer_state = HD44780_TRANSFER_LOW_NIBBLE_E_LOW;
					return;
				}

			case HD44780_TRANSFER_LOW_NIBBLE_E_LOW:
				// Lower E for low nibble - byte complete
				{
					uint8_t nibble = this->current_byte & 0x0F;
					this->transfer_complete = false;
					this->update_pins(nibble, this->current_rs, false);
					this->transfer_state = HD44780_TRANSFER_IDLE;

					// Set delay based on command
					if (this->current_byte == HD44780_CMD_CLEAR_DISPLAY ||
					    this->current_byte == HD44780_CMD_RETURN_HOME)
					{
						this->delay = HD44780_DELAY_LONG_CMD;
					}
					else
					{
						this->delay = HD44780_DELAY_SHORT_CMD;
					}
					this->init_step++;
					return;
				}

			case HD44780_TRANSFER_SINGLE_NIBBLE_E_LOW:
				// Lower E for single nibble
				this->transfer_complete = false;
				this->update_pins(this->single_nibble_value, false, false);
				this->transfer_state = HD44780_TRANSFER_IDLE;
				this->delay = HD44780_DELAY_SHORT_CMD;
				this->init_step++;
				return;

			default:
				this->transfer_state = HD44780_TRANSFER_IDLE;
				break;
		}
	}
	else if (this->state == HD44780_STATE_TRANSFER)
	{
		switch (this->transfer_state)
		{
			case HD44780_TRANSFER_IDLE:
				// Ready to send next byte from buffer
				if (this->buffer_write_in_progress)
				{
					// Check if we're doing partial or full buffer write
					if (this->buffer_write_partial)
					{
						// Partial buffer write mode
						if (this->buffer_write_chars_sent < this->buffer_write_length)
						{
							if (this->buffer_write_need_cursor)
							{
								// Send cursor positioning command
								// Calculate current absolute position
								uint16_t current_pos = this->buffer_write_start_pos + this->buffer_write_chars_sent;
								uint8_t current_row = current_pos / this->columns;
								uint8_t current_col = current_pos % this->columns;

								uint8_t row_offsets[] = {HD44780_ROW1_ADDR, HD44780_ROW2_ADDR,
								                         HD44780_ROW3_ADDR, HD44780_ROW4_ADDR};
								this->current_byte = HD44780_CMD_SET_DDRAM_ADDR | (row_offsets[current_row] + current_col);
								this->current_rs = false;
								uint8_t nibble = (this->current_byte >> 4) & 0x0F;
								this->transfer_complete = false;
								this->update_pins(nibble, false, true);
								this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
								this->buffer_write_need_cursor = false;
								return;
							}
							else
							{
								// Send character data
								this->current_byte = (uint8_t)this->buffer_ptr[this->buffer_write_chars_sent];
								this->current_rs = true;
								uint8_t nibble = (this->current_byte >> 4) & 0x0F;
								this->transfer_complete = false;
								this->update_pins(nibble, true, true);
								this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;

								this->buffer_write_chars_sent++;

								// Check if next character is on a different row (need cursor command)
								if (this->buffer_write_chars_sent < this->buffer_write_length)
								{
									uint16_t prev_pos = this->buffer_write_start_pos + this->buffer_write_chars_sent - 1;
									uint16_t next_pos = this->buffer_write_start_pos + this->buffer_write_chars_sent;
									uint8_t prev_row = prev_pos / this->columns;
									uint8_t next_row = next_pos / this->columns;

									if (next_row != prev_row)
									{
										this->buffer_write_need_cursor = true;
									}
								}

								// Check if we're done
								if (this->buffer_write_chars_sent >= this->buffer_write_length)
								{
									this->buffer_write_in_progress = false;
								}

								return;
							}
						}
						else
						{
							// Partial write complete
							this->buffer_write_in_progress = false;
							this->state = HD44780_STATE_IDLE;
						}
					}
					else
					{
						// Full buffer write mode (existing logic)
						if (this->buffer_write_index < this->buffer_write_total)
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
								uint8_t nibble = (this->current_byte >> 4) & 0x0F;
								this->transfer_complete = false;
								this->update_pins(nibble, false, true);
								this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
							}
							else
							{
								// Send character data
								uint16_t buffer_char_index = (this->buffer_write_index / (this->columns + 1)) *
								                             this->columns + (chars_written - 1);
								this->current_byte = (uint8_t)this->buffer_ptr[buffer_char_index];
								this->current_rs = true;
								uint8_t nibble = (this->current_byte >> 4) & 0x0F;
								this->transfer_complete = false;
								this->update_pins(nibble, true, true);
								this->transfer_state = HD44780_TRANSFER_HIGH_NIBBLE_E_LOW;
							}

							this->buffer_write_index++;

							// Check if we're done with the entire buffer
							if (this->buffer_write_index >= this->buffer_write_total)
							{
								this->buffer_write_in_progress = false;
							}

							return;
						}
						else
						{
							// Full buffer write complete
							this->buffer_write_in_progress = false;
							this->state = HD44780_STATE_IDLE;
						}
					}
				}
				else
				{
					// No more buffer data to process, return to IDLE
					this->state = HD44780_STATE_IDLE;
				}
				break;

			case HD44780_TRANSFER_HIGH_NIBBLE_E_LOW:
				// Lower E for high nibble
				{
					uint8_t nibble = (this->current_byte >> 4) & 0x0F;
					this->transfer_complete = false;
					this->update_pins(nibble, this->current_rs, false);
					this->transfer_state = HD44780_TRANSFER_LOW_NIBBLE_E_HIGH;
					return;
				}

			case HD44780_TRANSFER_LOW_NIBBLE_E_HIGH:
				// Raise E for low nibble
				{
					uint8_t nibble = this->current_byte & 0x0F;
					this->transfer_complete = false;
					this->update_pins(nibble, this->current_rs, true);
					this->transfer_state = HD44780_TRANSFER_LOW_NIBBLE_E_LOW;
					return;
				}

			case HD44780_TRANSFER_LOW_NIBBLE_E_LOW:
				// Lower E for low nibble - byte complete
				{
					uint8_t nibble = this->current_byte & 0x0F;
					this->transfer_complete = false;
					this->update_pins(nibble, this->current_rs, false);
					this->transfer_state = HD44780_TRANSFER_IDLE;
					this->delay = HD44780_DELAY_SHORT_CMD;
					return;
				}

			default:
				this->transfer_state = HD44780_TRANSFER_IDLE;
				break;
		}
	}
	else if (this->state == HD44780_STATE_IDLE)
	{
		// Just waiting, nothing to do
	}
}

void hd44780_transfer_complete(hd44780_data_t * this)
{
	// This callback is called when the I2C transfer to PCF8574 completes
	// Simply set the flag - state management happens in hd44780_main
	this->transfer_complete = true;
}

void hd44780_error(hd44780_data_t * this)
{
	this->state = HD44780_STATE_IDLE;
	this->transfer_state = HD44780_TRANSFER_IDLE;
	this->transfer_complete = false;
	this->buffer_write_in_progress = false;
	this->buffer_write_partial = false;
}

int hd44780_write_buffer(hd44780_data_t * this, const char * buffer)
{
	// Only accept new buffer write when in IDLE state and not already writing
	if (this->state != HD44780_STATE_IDLE || this->buffer_write_in_progress)
	{
		return -1;  // Error: busy or not ready
	}

	this->buffer_ptr = buffer;
	this->buffer_write_index = 0;
	// Total includes: rows * (1 command + columns characters)
	this->buffer_write_total = this->rows * (this->columns + 1);
	this->buffer_write_in_progress = true;
	this->buffer_write_partial = false;  // Full buffer write

	// Move to TRANSFER state to start processing the buffer
	this->state = HD44780_STATE_TRANSFER;
	this->transfer_state = HD44780_TRANSFER_IDLE;
	this->delay = 0;

	return 0;  // Success
}

int hd44780_write_buffer_at_position(hd44780_data_t * this, const char * buffer,
									uint16_t position, uint16_t length)
{
	// Only accept new buffer write when in IDLE state and not already writing
	if (this->state != HD44780_STATE_IDLE || this->buffer_write_in_progress)
	{
		return -1;  // Error: busy or not ready
	}

	// Validate parameters
	uint16_t total_screen_size = (uint16_t)this->rows * (uint16_t)this->columns;
	if (position >= total_screen_size || length == 0)
	{
		return -1;  // Error: invalid position or length
	}

	// Check that write doesn't exceed screen bounds
	if (position + length > total_screen_size)
	{
		return -1;  // Error: write would exceed screen size
	}

	// Set up partial buffer write
	this->buffer_ptr = buffer;
	this->buffer_write_start_pos = position;
	this->buffer_write_length = length;
	this->buffer_write_chars_sent = 0;
	this->buffer_write_need_cursor = true;  // Always start with cursor positioning
	this->buffer_write_in_progress = true;
	this->buffer_write_partial = true;

	// Move to TRANSFER state to start processing the buffer
	this->state = HD44780_STATE_TRANSFER;
	this->transfer_state = HD44780_TRANSFER_IDLE;
	this->delay = 0;

	return 0;  // Success
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
