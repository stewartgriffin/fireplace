/***********************************************************************************************************************
 *
 *            File: nvm_manager.c
 *      Created on: Mar 15, 2026
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "nvm_manager.h"
#include "stm32h5xx_hal.h"
#include <string.h>

/**************************************           DEFINES                    ******************************************/

/** Magic number written to flash to mark a valid NVM block. */
#define NVM_MAGIC           0xF1A5B10CUL

/**
 * Last flash sector on STM32H533RE (512 KB / 8 KB per sector = 64 sectors, 0-indexed).
 * Sector 63 starts at 0x0807E000. This sector must be excluded from the linker script
 * (reduce FLASH LENGTH from 512K to 504K in STM32H533xx_FLASH.ld).
 */
#define NVM_FLASH_SECTOR    63U
#define NVM_FLASH_ADDRESS   0x0807E000UL

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief On-flash layout. Must be exactly 16 bytes (one STM32H5 flash word).
 * The struct is 16-byte aligned to satisfy the FLASHWORD programming requirement.
 */
typedef struct __attribute__((aligned(16)))
{
    uint32_t magic;              ///< NVM_MAGIC when block is valid
    int16_t  highest_exhaust;    ///< Highest recorded exhaust temperature (°C)
    uint8_t  padding[10];        ///< Pad to 16 bytes
} nvm_layout_t;

/**************************************           STATIC VARIABLES           ******************************************/

static int16_t s_highest_exhaust = 0;
static bool    s_midnight_saved  = false;

/**************************************           STATIC FUNCTIONS           ******************************************/

static void load_from_flash(void)
{
    const nvm_layout_t *stored = (const nvm_layout_t *)NVM_FLASH_ADDRESS;
    if (stored->magic == NVM_MAGIC)
    {
        s_highest_exhaust = stored->highest_exhaust;
    }
}

static void save_to_flash(void)
{
    nvm_layout_t data __attribute__((aligned(16)));
    memset(&data, 0, sizeof(data));
    data.magic           = NVM_MAGIC;
    data.highest_exhaust = s_highest_exhaust;

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Banks     = FLASH_BANK_1,
        .Sector    = NVM_FLASH_SECTOR,
        .NbSectors = 1U,
    };
    uint32_t sector_error;
    HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, NVM_FLASH_ADDRESS, (uint32_t)&data);

    HAL_FLASH_Lock();
}

/**************************************           PUBLIC FUNCTIONS           ******************************************/

void nvm_manager_init(nvm_manager_data_t *nvm)
{
    (void)nvm;
    load_from_flash();
}

void nvm_manager_main(nvm_manager_data_t *nvm)
{
    time_data_t *t = nvm->get_time();

    if (t->hour == 0 && t->minute == 0)
    {
        if (!s_midnight_saved)
        {
            save_to_flash();
            s_midnight_saved = true;
        }
    }
    else
    {
        s_midnight_saved = false;
    }
}

void nvm_manager_set_highest_exhaust_temperature(int16_t temp)
{
    s_highest_exhaust = temp;
}

int16_t nvm_manager_get_highest_exhaust_temperature(void)
{
    return s_highest_exhaust;
}
