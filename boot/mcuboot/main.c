#include <stdio.h>

#include "build_id.h"

#include "board.h"
#include "boot_handoff.h"
#include "boot_security.h"
#include "bootutil/bootutil.h"
#include "bootutil/image.h"

/* Called by the ST-derived flash driver on an unrecoverable flash ECC error. */
void Error_Handler(void)
{
    printf("[bl2] flash error\r\n");
    while (1) {
    }
}

int main(void)
{
    struct boot_rsp rsp;
    fih_ret fih_rc;
    int ret;

    if (board_init() != 0) {
        while (1) {
        }
    }

    printf("\r\n[bl2] mcuboot build=%s\r\n", BUILD_ID);

    boot_security_init();

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
