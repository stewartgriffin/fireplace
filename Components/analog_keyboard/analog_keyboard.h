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
typedef struct
{
	int (*adc_start_conversion)(void);
	int (*adc_read)(uint32_t *value);
	void (*button_left_callback)(bool state);
	void (*button_up_callback)(bool state);
	void (*button_down_callback)(bool state);
	void (*button_right_callback)(bool state);
	void (*button_ok_callback)(bool state);
	uint32_t last_tick;
	uint32_t adc_value;
	bool conversion_in_progress;
	bool button_left_state;
	bool button_up_state;
	bool button_down_state;
	bool button_right_state;
	bool button_ok_state;
	bool button_left_prev_state;
	bool button_up_prev_state;
	bool button_down_prev_state;
	bool button_right_prev_state;
	bool button_ok_prev_state;
	bool button_left_flag;
	bool button_up_flag;
	bool button_down_flag;
	bool button_right_flag;
	bool button_ok_flag;
}analog_keyboard_data_t;

/**************************************           DEFINES                    ******************************************/
// ADC voltage range thresholds (measured values: left=64, up=646, down=1388, right=2096, ok=3040, off=4095)
// Generous margins added to account for component aging and corrosion
#define ANALOG_KEYBOARD_LEFT_MIN		0
#define ANALOG_KEYBOARD_LEFT_MAX		350

#define ANALOG_KEYBOARD_UP_MIN			351
#define ANALOG_KEYBOARD_UP_MAX			950

#define ANALOG_KEYBOARD_DOWN_MIN		951
#define ANALOG_KEYBOARD_DOWN_MAX		1750

#define ANALOG_KEYBOARD_RIGHT_MIN		1751
#define ANALOG_KEYBOARD_RIGHT_MAX		2600

#define ANALOG_KEYBOARD_OK_MIN			2601
#define ANALOG_KEYBOARD_OK_MAX			3500

#define ANALOG_KEYBOARD_OFF_MIN			3501

#define ANALOG_KEYBOARD_SAMPLE_PERIOD_MS	10

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void analog_keyboard_init(analog_keyboard_data_t * this,
						int (*adc_start_conversion)(void),
						int (*adc_read)(uint32_t *value),
						void (*button_left_callback)(bool state),
						void (*button_up_callback)(bool state),
						void (*button_down_callback)(bool state),
						void (*button_right_callback)(bool state),
						void (*button_ok_callback)(bool state));
void analog_keyboard_main(analog_keyboard_data_t * this);
void analog_keyboard_interrupt(analog_keyboard_data_t * this);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_ANALOG_KEYBOARD_ANALOG_KEYBOARD_H_ */
