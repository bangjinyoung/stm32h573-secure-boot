#include <stdio.h>

#include "build_id.h"

#include "board.h"
#ifdef BL2_SYSCLK_PLL250
#include "boot_clock.h"
#include "stm32h5xx_hal.h"
#endif
#include "boot_handoff.h"
#include "boot_rollback.h"
#include "boot_security.h"
#ifdef BL2_TIMING
#include "boot_timing.h"
#endif
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "mcuboot_config/mcuboot_config.h"

/* SBS HDPLSR value at HDPL2, where STiRoT starts BL2 (HAL SBS_HDPL_VALUE_2). */
#define BL2_HDPL2 0x8AU

#ifdef MCUBOOT_HW_ROLLBACK_PROT
#define BL2_ROLLBACK_NAME "hw"
#else
#define BL2_ROLLBACK_NAME "sw"
#endif

/* Called by the ST-derived flash driver on an unrecoverable flash ECC error. */
void Error_Handler(void)
{
    printf("[bl2] flash error\r\n");
    while (1) {
    }
}

int main(void)
{
    struct board_boot_info boot_info;
    struct boot_rsp rsp;
    fih_ret fih_rc;
    int ret;
#ifdef BL2_SYSCLK_PLL250
    int clock_ret;

    /* board_init() calls HAL_Init() again, which recomputes SystemCoreClock for SysTick and the console baud rate. */
    (void)HAL_Init();
    clock_ret = boot_clock_set_max();
#endif

    if (board_init() != 0) {
        while (1) {
        }
    }

    printf("\r\n[bl2] mcuboot build=%s rollback=%s\r\n", BUILD_ID, BL2_ROLLBACK_NAME);
#ifdef BL2_SYSCLK_PLL250
    printf("[bl2] sysclk=%lu hz (pll250 ret=%d)\r\n", SystemCoreClock, clock_ret);
#endif

    /*
     * With SW1=0 the core runs this image from SECBOOTADD without STiRoT, at HDPL1, between
     * debugger connects. Touch neither the slots nor the OBK then.
     */
    board_get_boot_info(&boot_info);
    if (boot_info.hdpl != BL2_HDPL2) {
        printf("[bl2] hdpl=0x%02lx, not started by stirot: stop\r\n", boot_info.hdpl);
        while (1) {
        }
    }

    boot_security_init();
    boot_rollback_init();

#ifdef BL2_TIMING
    boot_timing_start();
#endif
    FIH_CALL(boot_go, fih_rc, &rsp);
#ifdef BL2_TIMING
    boot_timing_report();
#endif
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        printf("[bl2] no bootable image\r\n");
        while (1) {
            board_led_toggle();
            board_delay_ms(100U);
        }
    }

    printf(
        "[bl2] image ok version=%u.%u.%u+%lu\r\n",
        rsp.br_hdr->ih_ver.iv_major,
        rsp.br_hdr->ih_ver.iv_minor,
        rsp.br_hdr->ih_ver.iv_revision,
        rsp.br_hdr->ih_ver.iv_build_num);

    ret = boot_handoff_jump(&rsp);
    printf("[bl2] handoff failed ret=%d\r\n", ret);

    while (1) {
    }
}
