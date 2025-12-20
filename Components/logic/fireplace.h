/***********************************************************************************************************************
 *
 *            File: fireplace.h
 *      Created on: Oct 25, 2025 9:55:58 PM
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef SRC_LOGIC_FIREPLACE_H_
#define SRC_LOGIC_FIREPLACE_H_

/**************************************           INCLUDE FILES              ******************************************/

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/
void fireplace_init(void);
void fireplace_main(void);
void clock_i2_interrupt(void);
/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* SRC_LOGIC_FIREPLACE_H_ */
