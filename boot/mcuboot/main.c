#include <stdio.h>

#include "build_id.h"

#include "board.h"
#include "boot_handoff.h"
#include "boot_rollback.h"
#include "boot_security.h"
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

    if (board_init() != 0) {
        while (1) {
        }
    }

    printf("\r\n[bl2] mcuboot build=%s rollback=%s\r\n", BUILD_ID, BL2_ROLLBACK_NAME);

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

    FIH_CALL(boot_go, fih_rc, &rsp);
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
