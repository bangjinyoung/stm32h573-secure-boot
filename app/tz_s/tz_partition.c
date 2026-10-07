#include "tz_partition.h"

#include <arm_cmse.h>
#include <errno.h>
#include <stdbool.h>

#include "flash_layout.h"
#include "stm32h5xx_hal.h"

#define TZ_NS_FLASH_START  (FLASH_BASE_ADDRESS_NS + FLASH_AREA_1_OFFSET)
#define TZ_NS_FLASH_END    (TZ_NS_FLASH_START + FLASH_AREA_1_SIZE)
#define TZ_NS_SRAM_START   SRAM3_BASE_NS
#define TZ_NS_SRAM_END     (SRAM3_BASE_NS + SRAM3_SIZE)
#define TZ_NS_PERIPH_START PERIPH_BASE_NS
#define TZ_NS_PERIPH_END   0x50000000U

struct tz_sau_region {
    uint32_t start;
    uint32_t end; /* exclusive */
    bool nsc;
};

typedef void __attribute__((cmse_nonsecure_call)) tz_ns_entry_t(void);

/*
 * Everything outside these regions stays Secure (the SAU default with TrustZone on, RM0481 3.6.3). The Non-secure
 * app gets its own primary slot, SRAM3 and the Non-secure peripheral alias; the other Non-secure slots stay
 * unreachable from it.
 */
static int tz_sau_init(void)
{
    const struct tz_sau_region regions[] = {
        {(uint32_t)__nsc_region_start, (uint32_t)__nsc_region_end, true},
        {TZ_NS_FLASH_START, TZ_NS_FLASH_END, false},
        {TZ_NS_SRAM_START, TZ_NS_SRAM_END, false},
        {TZ_NS_PERIPH_START, TZ_NS_PERIPH_END, false},
    };
    uint32_t region_count = (SAU->TYPE & SAU_TYPE_SREGION_Msk) >> SAU_TYPE_SREGION_Pos;
    uint32_t i;

    if ((sizeof(regions) / sizeof(regions[0])) > region_count) {
        return -ENOSPC;
    }

    SAU->CTRL = 0U;
    for (i = 0U; i < region_count; i++) {
        SAU->RNR = i;
        SAU->RLAR = 0U;
    }
    for (i = 0U; i < (sizeof(regions) / sizeof(regions[0])); i++) {
        SAU->RNR = i;
        SAU->RBAR = regions[i].start & SAU_RBAR_BADDR_Msk;
        SAU->RLAR = ((regions[i].end - 1U) & SAU_RLAR_LADDR_Msk) | (regions[i].nsc ? SAU_RLAR_NSC_Msk : 0U) | SAU_RLAR_ENABLE_Msk;
    }
    SAU->CTRL = SAU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();

    return 0;
}

/* All SRAMs reset Secure (RM0481 2.2.1); SRAM3 becomes Non-secure and unprivileged, block by block. */
static int tz_sram3_init(void)
{
    MPCBB_ConfigTypeDef mpcbb = {0};

    mpcbb.SecureRWIllegalMode = GTZC_MPCBB_SRWILADIS_ENABLE;
    mpcbb.InvertSecureState = GTZC_MPCBB_INVSECSTATE_NOT_INVERTED;
    mpcbb.AttributeConfig.MPCBB_LockConfig_array[0] = 0U;

    if (HAL_GTZC_MPCBB_ConfigMem(SRAM3_BASE, &mpcbb) != HAL_OK) {
        return -EIO;
    }
    return 0;
}

int tz_partition_init(void)
{
    int ret;

    /* A Non-secure violation reports as SecureFault with SFSR/SFAR intact instead of escalating to HardFault. */
    SCB->SHCSR |= SCB_SHCSR_SECUREFAULTENA_Msk;
    /* The Non-secure app is built for the FPU (-mfloat-abi=hard). */
    SCB->NSACR |= SCB_NSACR_CP10_Msk | SCB_NSACR_CP11_Msk;

    __HAL_RCC_GTZC1_CLK_ENABLE();
    ret = tz_sram3_init();
    if (ret != 0) {
        return ret;
    }

    return tz_sau_init();
}

int tz_partition_start_nonsecure(uint32_t vector_table)
{
    const uint32_t *vectors = (const uint32_t *)vector_table;
    uint32_t sp = vectors[0];
    uint32_t reset = vectors[1];
    tz_ns_entry_t *entry;

    /* VTOR needs the table aligned to its size rounded up to a power of two: 0x24C bytes on the H573. */
    if ((vector_table < TZ_NS_FLASH_START) || (vector_table >= TZ_NS_FLASH_END) || ((vector_table & 0x3FFU) != 0U)) {
        return -EINVAL;
    }
    if ((sp <= TZ_NS_SRAM_START) || (sp > TZ_NS_SRAM_END) || ((sp & 0x7U) != 0U)) {
        return -EINVAL;
    }
    if (((reset & 0x1U) == 0U) || ((reset & ~0x1U) < vector_table) || ((reset & ~0x1U) >= TZ_NS_FLASH_END)) {
        return -EINVAL;
    }

    SCB_NS->VTOR = vector_table;
    __TZ_set_MSP_NS(sp);
    entry = (tz_ns_entry_t *)cmse_nsfptr_create(reset);
    entry();

    return -EFAULT;
}
