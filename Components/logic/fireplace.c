/***********************************************************************************************************************
 *
 *            File: fireplace.c
 *      Created on: Oct 25, 2025 9:55:48 PM
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"
#include "stm32h5xx_hal.h"
#include "max6675.h"
#include "spi.h"
#include "adc.h"

#include "ds3231.h"
#include "pcf8574.h"
#include "hd44780.h"
#include "analog_keyboard.h"
#include "ui.h"
#include "i2c.h"
#include "gui.h"
#include "flap_controller.h"
#include "daily_schedule.h"

/**************************************           DEFINES                    ******************************************/

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

// Ventilation schedule configuration
static const daily_schedule_entry_t ventilation_schedule[] = {
	{0, 0, 10},    // Midnight: 10%
	{5, 0, 3},     // 5:00 AM: 3%
	{15, 0, 0},    // 3:00 PM: 0%
	{22, 0, 10}    // 10:00 PM: 10%
};

static const daily_schedule_config_t ventilation_schedule_config = {
	.entries = ventilation_schedule,
	.num_entries = sizeof(ventilation_schedule) / sizeof(ventilation_schedule[0])
};

/**************************************           LOCAL VARIABLES            ******************************************/
max6675_data_t thermocouple;
int current_temperature = 0;

ds3231_data_t clock;
pcf8574_data_t gpio_expander;
hd44780_data_t display;
analog_keyboard_data_t keyboard;
ui_data_t ui;

flap_controller_data_t fireplace_flap;
flap_controller_data_t ventilation_flap;

daily_schedule_data_t ventilation_daily_schedule;

// Test variables: cycle fireplace flap 0->100->0% in 10% steps every 6s
static uint8_t  test_fireplace_position  = 0;
static int8_t   test_fireplace_direction = 1;   // +1 = opening, -1 = closing
static uint32_t test_fireplace_last_tick = 0;

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size);
int clock_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
int clock_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
int gpio_expander_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size);
void display_update_pins(uint8_t d4_d7, bool rs, bool e);
void fireplace_update_gui(void);
void gpio_expander_write_complete_callback(void);
int keyboard_adc_start_conversion(void);
int keyboard_adc_read(uint32_t *value);
void keyboard_button_left_callback(bool state);
void keyboard_button_up_callback(bool state);
void keyboard_button_down_callback(bool state);
void keyboard_button_right_callback(bool state);
void keyboard_button_ok_callback(bool state);
void ui_shift_focus_left_callback(void);
void ui_shift_focus_right_callback(void);
void ui_increase_time_callback(void);
void ui_decrease_time_callback(void);
void ui_time_edit_mode_callback(bool enter);
void ui_wrapper_set_input_up(bool state);
void ui_wrapper_set_input_down(bool state);
void ui_wrapper_set_input_left(bool state);
void ui_wrapper_set_input_right(bool state);
void ui_wrapper_set_input_ok(bool state);
void fireplace_flap_set_open_pin(uint8_t state);
void fireplace_flap_set_close_pin(uint8_t state);
void ventilation_flap_set_open_pin(uint8_t state);
void ventilation_flap_set_close_pin(uint8_t state);
int gui_update_partial_screen_wrapper(const char *buffer, uint16_t position, uint16_t length);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void fireplace_init(void)
{
	max6675_init(&thermocouple, thermocouple_spi_send_receive_wrapper);
	ds3231_init(&clock, clock_i2c_receive, clock_i2c_send);
	pcf8574_init(&gpio_expander, gpio_expander_i2c_send, gpio_expander_write_complete_callback);
	hd44780_init(&display, 4, 20, display_update_pins);
	analog_keyboard_init(&keyboard,
						keyboard_adc_start_conversion,
						keyboard_adc_read,
						keyboard_button_left_callback,
						keyboard_button_up_callback,
						keyboard_button_down_callback,
						keyboard_button_right_callback,
						keyboard_button_ok_callback);
	ui_init(&ui,
			ui_shift_focus_left_callback,
			ui_shift_focus_right_callback,
			ui_increase_time_callback,
			ui_decrease_time_callback,
			ui_time_edit_mode_callback);

	// Initialize GUI with partial screen update callback
	gui_init(gui_update_partial_screen_wrapper);

	// Initialize flap controllers
	flap_controller_init(&fireplace_flap, fireplace_flap_set_open_pin, fireplace_flap_set_close_pin, FLAP_DEFAULT_OPEN_TRAVEL_TIME_MS, FLAP_DEFAULT_CLOSE_TRAVEL_TIME_MS);
	flap_controller_init(&ventilation_flap, ventilation_flap_set_open_pin, ventilation_flap_set_close_pin, FLAP_DEFAULT_OPEN_TRAVEL_TIME_MS, FLAP_DEFAULT_CLOSE_TRAVEL_TIME_MS);

	// Initialize ventilation schedule
	daily_schedule_init(&ventilation_daily_schedule, &ventilation_schedule_config);
}

void fireplace_main(void)
{
	// All components now manage their own timing using HAL_GetTick()
	// Just call them as fast as possible - they will handle their own intervals
	max6675_main(&thermocouple);
	ds3231_main(&clock);
	pcf8574_main(&gpio_expander);
	hd44780_main(&display);
	analog_keyboard_main(&keyboard);
	ui_main_function(&ui);
	gui_main();
	fireplace_update_gui();

	// Update daily schedule
	daily_schedule_main(&ventilation_daily_schedule, *ds3231_get_time(&clock));

	// Update flap controllers
	flap_controller_main(&fireplace_flap);
	flap_controller_main(&ventilation_flap);

	// // Test: cycle fireplace flap 0->100->0% in 10% steps every 7s
	// uint32_t now = HAL_GetTick();
	// if (now - test_fireplace_last_tick >= 7000U)
	// {
	// 	test_fireplace_last_tick = now;
	// 	flap_controller_set_position(&fireplace_flap, test_fireplace_position);

	// 	test_fireplace_position += test_fireplace_direction * 10;
	// 	if (test_fireplace_position >= 100)
	// 	{
	// 		test_fireplace_position  = 100;
	// 		test_fireplace_direction = -1;
	// 	}
	// 	else if (test_fireplace_position == 0 && test_fireplace_direction == -1)
	// 	{
	// 		test_fireplace_direction = 1;
	// 	}
	// }

	current_temperature = max6675_get_temperature(&thermocouple);
}

void clock_i2c_interrupt(void)
{
	ds3231_interrupt(&clock);
}

void gpio_expander_i2c_interrupt(void)
{
	// pcf8574_interrupt will call the registered callback if this was a write operation
	pcf8574_interrupt(&gpio_expander);
}

void display_i2c_interrupt(void)
{
	hd44780_transfer_complete(&display);
}

void thermocouple_spi_interrupt(void)
{
	max6675_spi_irq_handler(&thermocouple);
}

void keyboard_adc_interrupt(void)
{
	analog_keyboard_interrupt(&keyboard);
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
int thermocouple_spi_send_receive_wrapper(uint8_t * tx_buffer, uint8_t * rx_buffer, uint16_t size)
{
	return HAL_SPI_TransmitReceive_IT(&hspi2, tx_buffer, rx_buffer, size);
}

int clock_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Mem_Write_IT(&hi2c2, 0x68 << 1, mem_addr, 1, data, data_size);
}

int clock_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Mem_Read_IT(&hi2c2, 0x68 << 1, mem_addr, 1, data, data_size);
}

int gpio_expander_i2c_send(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Master_Transmit_IT(&hi2c3, 0x27 << 1, data, data_size);
}

int gpio_expander_i2c_receive(uint8_t mem_addr, uint8_t *data, uint16_t data_size)
{
	return HAL_I2C_Master_Receive_IT(&hi2c3, 0x27 << 1, data, data_size);
}

void display_update_pins(uint8_t d4_d7, bool rs, bool e)
{
	// Pack HD44780 control signals into PCF8574 byte
	// Bit mapping: [D7 D6 D5 D4 BL E RW RS]
	uint8_t output = 0;

	// Data lines D4-D7 (bits 4-7)
	output |= (d4_d7 & 0x0F) << 4;

	// RS - Register Select (bit 0)
	if (rs)
	{
		output |= 0x01;
	}

	// RW - Read/Write (bit 1) - always 0 for write
	// output |= 0x00;

	// E - Enable (bit 2)
	if (e)
	{
		output |= 0x04;
	}

	// BL - Backlight (bit 3) - always on
	output |= 0x08;

	// Write to PCF8574
	pcf8574_write(&gpio_expander, output);
}

void fireplace_update_gui(void)
{
	gui_set_time(ds3231_get_time(&clock));

	gui_set_fireplace(flap_controller_get_position(&fireplace_flap));

	gui_set_ventilation(flap_controller_get_position(&ventilation_flap));

	gui_set_ppm(20);

	gui_set_fireplace_temperature(max6675_get_temperature(&thermocouple));
}

void gpio_expander_write_complete_callback(void)
{
	// Called when PCF8574 I2C write completes
	// This notifies HD44780 that the pin update is complete
	hd44780_transfer_complete(&display);
}

int keyboard_adc_start_conversion(void)
{
	return HAL_ADC_Start_IT(&hadc1);
}

int keyboard_adc_read(uint32_t *value)
{
	*value = HAL_ADC_GetValue(&hadc1);
	return 0;
}

void keyboard_button_left_callback(bool state)
{
	ui_wrapper_set_input_left(state);
}

void keyboard_button_up_callback(bool state)
{
	ui_wrapper_set_input_up(state);
}

void keyboard_button_down_callback(bool state)
{
	ui_wrapper_set_input_down(state);
}

void keyboard_button_right_callback(bool state)
{
	ui_wrapper_set_input_right(state);
}

void keyboard_button_ok_callback(bool state)
{
	ui_wrapper_set_input_ok(state);
}

void ui_shift_focus_left_callback(void)
{
	gui_shift_focus_left();
}

void ui_shift_focus_right_callback(void)
{
	gui_shift_focus_right();
}

void ui_increase_time_callback(void)
{
	gui_focus_t focus = gui_get_focus();

	if (focus == GUI_FOCUS_NONE)
	{
		return;
	}

	ds3231_time_param_t param;
	switch (focus)
	{
		case GUI_FOCUS_HOUR:
			param = DS3231_PARAM_HOUR;
			break;
		case GUI_FOCUS_MINUTE:
			param = DS3231_PARAM_MINUTE;
			break;
		case GUI_FOCUS_SECOND:
			param = DS3231_PARAM_SECOND;
			break;
		case GUI_FOCUS_DAY:
			param = DS3231_PARAM_DAY;
			break;
		case GUI_FOCUS_MONTH:
			param = DS3231_PARAM_MONTH;
			break;
		case GUI_FOCUS_YEAR:
			param = DS3231_PARAM_YEAR;
			break;
		default:
			return;
	}

	ds3231_adjust_time(&clock, param, DS3231_ADJUST_UP);
	ds3231_set_time(&clock, ds3231_get_time(&clock));
}

void ui_decrease_time_callback(void)
{
	gui_focus_t focus = gui_get_focus();

	if (focus == GUI_FOCUS_NONE)
	{
		return;
	}

	ds3231_time_param_t param;
	switch (focus)
	{
		case GUI_FOCUS_HOUR:
			param = DS3231_PARAM_HOUR;
			break;
		case GUI_FOCUS_MINUTE:
			param = DS3231_PARAM_MINUTE;
			break;
		case GUI_FOCUS_SECOND:
			param = DS3231_PARAM_SECOND;
			break;
		case GUI_FOCUS_DAY:
			param = DS3231_PARAM_DAY;
			break;
		case GUI_FOCUS_MONTH:
			param = DS3231_PARAM_MONTH;
			break;
		case GUI_FOCUS_YEAR:
			param = DS3231_PARAM_YEAR;
			break;
		default:
			return;
	}

	ds3231_adjust_time(&clock, param, DS3231_ADJUST_DOWN);
	ds3231_set_time(&clock, ds3231_get_time(&clock));
}

void ui_time_edit_mode_callback(bool enter)
{
	if (enter)
	{
		// Enter time edit mode
		gui_time_edit_mode(true);
	}
	else
	{
		// Exit time edit mode
		gui_time_edit_mode(false);
	}
}

void ui_wrapper_set_input_up(bool state)
{
	ui_set_input_up(&ui, state);
}

void ui_wrapper_set_input_down(bool state)
{
	ui_set_input_down(&ui, state);
}

void ui_wrapper_set_input_left(bool state)
{
	ui_set_input_left(&ui, state);
}

void ui_wrapper_set_input_right(bool state)
{
	ui_set_input_right(&ui, state);
}

void ui_wrapper_set_input_ok(bool state)
{
	ui_set_input_ok(&ui, state);
}

void fireplace_flap_set_open_pin(uint8_t state)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5,  state ? GPIO_PIN_SET : GPIO_PIN_RESET );
}

void fireplace_flap_set_close_pin(uint8_t state)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10,  state ? GPIO_PIN_SET : GPIO_PIN_RESET );
}

void ventilation_flap_set_open_pin(uint8_t state)
{
	// HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,   state ? GPIO_PIN_SET : GPIO_PIN_RESET );
}

void ventilation_flap_set_close_pin(uint8_t state)
{
	// HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6,  state ? GPIO_PIN_SET : GPIO_PIN_RESET );
}

int gui_update_partial_screen_wrapper(const char *buffer, uint16_t position, uint16_t length)
{
	// Wrapper for hd44780_write_buffer_at_position
	// Returns 0 on success, -1 if display is busy
	return hd44780_write_buffer_at_position(&display, buffer, position, length);
}
