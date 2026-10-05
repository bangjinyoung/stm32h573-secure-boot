#include "boot_sau.h"

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

/* Same Non-secure ranges as ST OEMuRoT's sau_init_cfg, for a single Secure image. */
static const struct boot_sau_region boot_sau_regions[] = {
    /* The flash driver programs and erases the Non-secure secondary slot through FLASH_NS. */
    {(uint32_t)FLASH_NS, (uint32_t)FLASH_NS + 0xFFFFU},
    /* The flash driver reads the secondary slot through the Non-secure flash alias. */
    {FLASH_BASE_ADDRESS_NS + FLASH_AREA_1_OFFSET, FLASH_BASE_ADDRESS_NS + FLASH_AREA_1_OFFSET + FLASH_AREA_1_SIZE - 1U},
    {BOOT_SAU_ENGI_BASE_NS, BOOT_SAU_ENGI_BASE_NS + BOOT_SAU_ENGI_SIZE - 1U},
    /* RSS library function table used for the HDPL3 handoff. */
    {RSSLIB_SYS_FLASH_NS_PFUNC_START, RSSLIB_SYS_FLASH_NS_PFUNC_END},
};

void boot_sau_init(void)
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
