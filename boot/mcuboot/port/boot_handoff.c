#include "boot_handoff.h"

#include <errno.h>
#include <stdio.h>

#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "flash_layout.h"
#include "stm32h5xx.h"

#define BOOT_HANDOFF_SRAM_START SRAM1_BASE
#define BOOT_HANDOFF_SRAM_END   (SRAM1_BASE + 0x000A0000U)

/* MPU region RSS JumpHDPLvl3 enables for the image, as in ST OEMuRoT (region 1). */
#define BOOT_HANDOFF_MPU_APP_REGION 1U
#define BOOT_HANDOFF_MPU_APP_ATTR   0U

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

/*
 * Clears the whole SRAM (BL2 data, heap and this stack), clears the registers that could carry
 * BL2 state, then calls RSS JumpHDPLvl3(vector_table, mpu_region). RSS raises HDPL to 3, enables
 * the MPU region and starts the image; it returns only on a parameter error, and then the core
 * stays in the loop below with the RSS status in r0 for SWD to read.
 */
__attribute__((naked, noreturn)) static void boot_handoff_rss_jump(__attribute__((unused)) uint32_t rss_jump, __attribute__((unused)) uint32_t vector_table,
                                                                       __attribute__((unused)) uint32_t mpu_region)
{
    __ASM volatile(".syntax unified            \n"
                   "mov     r4, r0             \n"
                   "mov     r5, r1             \n"
                   "mov     r6, r2             \n"
                   "ldr     r0, =%c0           \n"
                   "ldr     r1, =_estack       \n"
                   "movs    r2, #0             \n"
                   "1:                         \n"
                   "str     r2, [r0], #4       \n"
                   "cmp     r0, r1             \n"
                   "blo     1b                 \n"
                   "mov     r12, r4            \n"
                   "mov     r0, r5             \n"
                   "mov     r1, r6             \n"
                   "movs    r2, #0             \n"
                   "mov     r3, r2             \n"
                   "mov     r4, r2             \n"
                   "mov     r5, r2             \n"
                   "mov     r6, r2             \n"
                   "mov     r7, r2             \n"
                   "mov     r8, r2             \n"
                   "mov     r9, r2             \n"
                   "mov     r10, r2            \n"
                   "mov     r11, r2            \n"
                   "blx     r12                \n"
                   "2:                         \n"
                   "b       2b                 \n"
                   :
                   : "i"(SRAM1_BASE));
}

/* Configured but left disabled: RSS enables it after raising HDPL, so only the image runs. */
static void boot_handoff_set_app_region(void)
{
    uint32_t base = FLASH_BASE_ADDRESS + FLASH_AREA_0_OFFSET;
    uint32_t limit = base + FLASH_AREA_0_SIZE - 1U;

    ARM_MPU_SetMemAttr(BOOT_HANDOFF_MPU_APP_ATTR, ARM_MPU_ATTR(ARM_MPU_ATTR_MEMORY_(1U, 0U, 1U, 0U), ARM_MPU_ATTR_MEMORY_(1U, 0U, 1U, 0U)));
    MPU->RNR = BOOT_HANDOFF_MPU_APP_REGION;
    MPU->RBAR = ARM_MPU_RBAR(base, ARM_MPU_SH_NON, 1U, 0U, 0U);
    MPU->RLAR = (limit & MPU_RLAR_LIMIT_Msk) | ((BOOT_HANDOFF_MPU_APP_ATTR << MPU_RLAR_AttrIndx_Pos) & MPU_RLAR_AttrIndx_Msk);
    __DSB();
    __ISB();
}

int boot_handoff_jump(const struct boot_rsp *rsp)
{
    const struct boot_handoff_vectors *vectors;
    uint32_t rss_jump;
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

    rss_jump = (uint32_t)RSSLIB_PFUNC->S.JumpHDPLvl3;

    printf("[bl2] hdpl=0x%02lx jump to 0x%08lx sp=0x%08lx reset=0x%08lx via rss 0x%08lx\r\n", SBS->HDPLSR & SBS_HDPLSR_HDPL, (uint32_t)vectors,
           vectors->initial_sp, vectors->reset_handler, rss_jump);

    boot_handoff_quiesce();
    boot_handoff_set_app_region();
    __set_MSPLIM(0U);
    __set_PSPLIM(0U);

    /* Every interrupt source is disabled above, and the application startup does not clear PRIMASK. */
    __enable_irq();
    boot_handoff_rss_jump(rss_jump, (uint32_t)vectors, BOOT_HANDOFF_MPU_APP_REGION);
}
