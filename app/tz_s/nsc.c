#include "tz_nsc.h"

#include "flash_layout.h"
#include "stm32h5xx.h"

/* MCUboot image header: struct image_version starts at offset 20 (bootutil/image.h). */
#define TZ_NSC_IMAGE_HEADER    (FLASH_BASE_ADDRESS + FLASH_AREA_0_OFFSET)
#define TZ_NSC_VERSION_OFFSET  20U

uint32_t __attribute__((cmse_nonsecure_entry)) secure_boot_info(void)
{
    const volatile uint8_t *version = (const volatile uint8_t *)(TZ_NSC_IMAGE_HEADER + TZ_NSC_VERSION_OFFSET);
    uint32_t hdpl = SBS->HDPLSR & SBS_HDPLSR_HDPL;

    return (hdpl & 0xFFU) | ((uint32_t)version[0] << 8) | ((uint32_t)version[1] << 16);
}
