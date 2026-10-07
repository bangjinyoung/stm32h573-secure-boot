#include "boot_security.h"

#include <stdint.h>

#include "flash_layout.h"
#include "stm32h5xx.h"

/* Engineering bits the RSS library reads (ST OEMiROT_Boot flash_layout.h ENGI_BASE_NS). */
#define BOOT_SAU_ENGI_BASE_NS 0x08FFF800U
#define BOOT_SAU_ENGI_SIZE    0x40U

struct boot_sau_region {
    uint32_t base;
    uint32_t limit;
};

/* Non-secure ranges as in ST OEMuRoT's sau_init_cfg; the flash range covers every slot but the image 0 primary. */
static const struct boot_sau_region boot_sau_regions[] = {
    /* The flash driver programs and erases the Non-secure secondary slot through FLASH_NS. */
    {(uint32_t)FLASH_NS, (uint32_t)FLASH_NS + 0xFFFFU},
    /* The flash driver reads the Non-secure slots through the Non-secure flash alias. */
    {FLASH_BASE_ADDRESS_NS + FLASH_AREA_1_OFFSET, FLASH_BASE_ADDRESS_NS + FLASH_AREA_END_OFFSET - 1U},
    {BOOT_SAU_ENGI_BASE_NS, BOOT_SAU_ENGI_BASE_NS + BOOT_SAU_ENGI_SIZE - 1U},
    /* RSS library function table used for the HDPL3 handoff. */
    {RSSLIB_SYS_FLASH_NS_PFUNC_START, RSSLIB_SYS_FLASH_NS_PFUNC_END},
};

/*
 * STiRoT starts HDPL2 with the MPU on, PRIVDEFENA clear and regions for its own idea of HDPL2
 * (no Non-secure flash, no Non-secure peripherals). ST OEMuRoT clears them first too
 * (LL_SECU_DisableCleanMpu) before it programs its own set.
 */
static void boot_mpu_clear(void)
{
    uint32_t region_count = (MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;
    uint32_t i;

    __DMB();
    MPU->CTRL = 0U;

    for (i = 0U; i < region_count; i++) {
        MPU->RNR = i;
        MPU->RLAR = 0U;
        MPU->RBAR = 0U;
    }

    __DSB();
    __ISB();
}

static void boot_sau_init(void)
{
    uint32_t region_count = (SAU->TYPE & SAU_TYPE_SREGION_Msk) >> SAU_TYPE_SREGION_Pos;
    uint32_t i;

    SAU->CTRL = 0U;

    for (i = 0U; i < region_count; i++) {
        SAU->RNR = i;
        SAU->RLAR = 0U;
    }

    for (i = 0U; i < (sizeof(boot_sau_regions) / sizeof(boot_sau_regions[0])); i++) {
        SAU->RNR = i;
        SAU->RBAR = boot_sau_regions[i].base & SAU_RBAR_BADDR_Msk;
        SAU->RLAR = (boot_sau_regions[i].limit & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;
    }

    SAU->CTRL = SAU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
}

void boot_security_init(void)
{
    boot_mpu_clear();
    boot_sau_init();
}
