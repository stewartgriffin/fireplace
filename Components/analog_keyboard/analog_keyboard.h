/***********************************************************************************************************************
 *
 *            File: analog_keyboard.h
 *      Created on: Dec 23, 2025
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef SRC_DRIVERS_ANALOG_KEYBOARD_ANALOG_KEYBOARD_H_
#define SRC_DRIVERS_ANALOG_KEYBOARD_ANALOG_KEYBOARD_H_

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief Analog keyboard driver data structure
 * Reads 5 buttons through a single ADC channel using resistor divider network
 * Each button produces a unique voltage level that is read via ADC
 * Sampling rate: 10ms
 */
typedef struct
{
	/**
	 * @brief Function pointer to start ADC conversion (interrupt-driven)
	 * @return 0 on success, error code otherwise
	 */
	int (*adc_start_conversion)(void);

	/**
	 * @brief Function pointer to read ADC value after conversion completes
	 * @param value Pointer to store ADC result (12-bit: 0-4095)
	 * @return 0 on success, error code otherwise
	 */
	int (*adc_read)(uint32_t *value);

	/**
	 * @brief Callback when left button state changes
	 * @param state true if pressed, false if released
	 */
	void (*button_left_callback)(bool state);

	/**
	 * @brief Callback when up button state changes
	 * @param state true if pressed, false if released
	 */
	void (*button_up_callback)(bool state);

	/**
	 * @brief Callback when down button state changes
	 * @param state true if pressed, false if released
	 */
	void (*button_down_callback)(bool state);

	/**
	 * @brief Callback when right button state changes
	 * @param state true if pressed, false if released
	 */
	void (*button_right_callback)(bool state);

	/**
	 * @brief Callback when OK button state changes
	 * @param state true if pressed, false if released
	 */
	void (*button_ok_callback)(bool state);

	uint32_t last_tick;             ///< Last HAL_GetTick() value for sampling interval
	uint32_t adc_value;             ///< Last read ADC value (0-4095)
	bool conversion_in_progress;    ///< True if ADC conversion is active

	// Current button states (decoded from ADC value)
	bool button_left_state;         ///< Current state of left button
	bool button_up_state;           ///< Current state of up button
	bool button_down_state;         ///< Current state of down button
	bool button_right_state;        ///< Current state of right button
	bool button_ok_state;           ///< Current state of OK button

	// Previous button states (for edge detection)
	bool button_left_prev_state;    ///< Previous state of left button
	bool button_up_prev_state;      ///< Previous state of up button
	bool button_down_prev_state;    ///< Previous state of down button
	bool button_right_prev_state;   ///< Previous state of right button
	bool button_ok_prev_state;      ///< Previous state of OK button

	// Flags to prevent multiple callbacks per state change
	bool button_left_flag;          ///< Left button callback already sent
	bool button_up_flag;            ///< Up button callback already sent
	bool button_down_flag;          ///< Down button callback already sent
	bool button_right_flag;         ///< Right button callback already sent
	bool button_ok_flag;            ///< OK button callback already sent
}analog_keyboard_data_t;

/**************************************           DEFINES                    ******************************************/

/**
 * @brief ADC voltage range thresholds for button detection
 *
 * Measured reference values (12-bit ADC: 0-4095):
 * - Left:   64
 * - Up:     646
 * - Down:   1388
 * - Right:  2096
 * - OK:     3040
 * - None:   4095
 *
 * Generous margins added to account for component aging and corrosion
 */
#define ANALOG_KEYBOARD_LEFT_MIN		0       ///< Left button ADC minimum threshold
#define ANALOG_KEYBOARD_LEFT_MAX		350     ///< Left button ADC maximum threshold

#define ANALOG_KEYBOARD_UP_MIN			351     ///< Up button ADC minimum threshold
#define ANALOG_KEYBOARD_UP_MAX			950     ///< Up button ADC maximum threshold

#define ANALOG_KEYBOARD_DOWN_MIN		951     ///< Down button ADC minimum threshold
#define ANALOG_KEYBOARD_DOWN_MAX		1750    ///< Down button ADC maximum threshold

#define ANALOG_KEYBOARD_RIGHT_MIN		1751    ///< Right button ADC minimum threshold
#define ANALOG_KEYBOARD_RIGHT_MAX		2600    ///< Right button ADC maximum threshold

#define ANALOG_KEYBOARD_OK_MIN			2601    ///< OK button ADC minimum threshold
#define ANALOG_KEYBOARD_OK_MAX			3500    ///< OK button ADC maximum threshold

#define ANALOG_KEYBOARD_OFF_MIN			3501    ///< No button pressed ADC threshold

#define ANALOG_KEYBOARD_SAMPLE_PERIOD_MS	10  ///< ADC sampling interval in milliseconds

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialize analog keyboard driver
 * @param this Pointer to analog keyboard data structure
 * @param adc_start_conversion Function pointer to start ADC conversion (interrupt-driven)
 * @param adc_read Function pointer to read ADC result after conversion completes
 * @param button_left_callback Callback invoked when left button state changes
 * @param button_up_callback Callback invoked when up button state changes
 * @param button_down_callback Callback invoked when down button state changes
 * @param button_right_callback Callback invoked when right button state changes
 * @param button_ok_callback Callback invoked when OK button state changes
 */
void analog_keyboard_init(analog_keyboard_data_t * this,
						int (*adc_start_conversion)(void),
						int (*adc_read)(uint32_t *value),
						void (*button_left_callback)(bool state),
						void (*button_up_callback)(bool state),
						void (*button_down_callback)(bool state),
						void (*button_right_callback)(bool state),
						void (*button_ok_callback)(bool state));

/**
 * @brief Main function - call periodically to sample buttons
 * Triggers ADC conversion every 10ms and decodes button states
 * Invokes callbacks when button states change
 * @param this Pointer to analog keyboard data structure
 */
void analog_keyboard_main(analog_keyboard_data_t * this);

/**
 * @brief ADC conversion complete interrupt callback
 * Call from ADC interrupt handler when conversion completes
 * Reads ADC value and decodes which button is pressed
 * @param this Pointer to analog keyboard data structure
 */
void analog_keyboard_interrupt(analog_keyboard_data_t * this);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_ANALOG_KEYBOARD_ANALOG_KEYBOARD_H_ */
