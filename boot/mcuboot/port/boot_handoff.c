#include "boot_handoff.h"

#include <errno.h>
#include <stdio.h>

#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "flash_layout.h"
#include "stm32h5xx.h"

#define BOOT_HANDOFF_SRAM_START 0x20000000U
#define BOOT_HANDOFF_SRAM_END   0x200A0000U

struct boot_handoff_vectors {
    uint32_t initial_sp;
    uint32_t reset_handler;
};

static int boot_handoff_validate_rsp(const struct boot_rsp *rsp)
{
    const struct image_header *hdr = rsp->br_hdr;

    if (hdr == NULL) {
        return -EINVAL;
    }

    if ((rsp->br_flash_dev_id != FLASH_AREA_0_DEVICE_ID) || (rsp->br_image_off != FLASH_AREA_0_OFFSET)) {
        return -EINVAL;
    }

    if (hdr->ih_hdr_size != BL2_IMAGE_HEADER_SIZE) {
        return -EINVAL;
    }

    if ((hdr->ih_img_size < sizeof(struct boot_handoff_vectors)) || (hdr->ih_img_size > (FLASH_AREA_0_SIZE - hdr->ih_hdr_size))) {
        return -EINVAL;
    }

    return 0;
}

static int boot_handoff_validate_vectors(const struct boot_handoff_vectors *vectors, uint32_t image_size)
{
    uint32_t image_start = (uint32_t)vectors;
    uint32_t reset_address = vectors->reset_handler & ~0x1U;

    if ((vectors->initial_sp <= BOOT_HANDOFF_SRAM_START) || (vectors->initial_sp > BOOT_HANDOFF_SRAM_END) || ((vectors->initial_sp & 0x7U) != 0U)) {
        return -EINVAL;
    }

    if (((vectors->reset_handler & 0x1U) == 0U) || (reset_address < image_start) || (reset_address >= (image_start + image_size))) {
        return -EINVAL;
    }

    return 0;
}

static void boot_handoff_quiesce(void)
{
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    for (uint32_t i = 0U; i < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
}

int boot_handoff_jump(const struct boot_rsp *rsp)
{
    const struct boot_handoff_vectors *vectors;
    void (*reset_handler)(void);
    int ret;

    ret = boot_handoff_validate_rsp(rsp);
    if (ret != 0) {
        return ret;
    }

    vectors = (const struct boot_handoff_vectors *)(FLASH_BASE_ADDRESS + rsp->br_image_off + rsp->br_hdr->ih_hdr_size);

    ret = boot_handoff_validate_vectors(vectors, rsp->br_hdr->ih_img_size);
    if (ret != 0) {
        return ret;
    }

    printf("[bl2] jump to 0x%08lx sp=0x%08lx reset=0x%08lx\r\n", (uint32_t)vectors, vectors->initial_sp, vectors->reset_handler);

    reset_handler = (void (*)(void))vectors->reset_handler;

    boot_handoff_quiesce();

    SCB->VTOR = (uint32_t)vectors;
    __set_MSPLIM(0U);
    __set_PSPLIM(0U);
    __set_MSP(vectors->initial_sp);
    __DSB();
    __ISB();

    /* Every interrupt source is disabled above, and the application startup does not clear PRIMASK. */
    __enable_irq();
    reset_handler();

    return -EFAULT;
}
