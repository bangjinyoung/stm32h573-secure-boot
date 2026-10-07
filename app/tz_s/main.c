#include <stdio.h>

#include "build_id.h"

#include "board.h"
#include "flash_layout.h"
#include "stm32h5xx.h"
#include "tz_nsc.h"
#include "tz_partition.h"

/* The Non-secure vector table follows the image 1 header (MCUboot, --header-size 0x400). */
#define TZ_NS_VECTOR_TABLE (FLASH_BASE_ADDRESS_NS + FLASH_AREA_1_OFFSET + 0x400U)

static void halt(void)
{
    while (1) {
        board_led_toggle();
        board_delay_ms(100U);
    }
}

int main(void)
{
    struct board_boot_info boot_info;
    int ret;

    if (board_init() != 0) {
        while (1) {
        }
    }

    board_get_boot_info(&boot_info);
    printf("\r\n[s] stm32h573 secure app build=%s\r\n", BUILD_ID);
    printf("[s] hdpl=0x%02lx vtor=0x%08lx sp=0x%08lx\r\n", boot_info.hdpl, boot_info.vtor, boot_info.initial_sp);

    ret = tz_partition_init();
    if (ret != 0) {
        printf("[s] partition failed ret=%d\r\n", ret);
        halt();
    }
    printf("[s] sau ctrl=0x%08lx nsc=0x%08lx-0x%08lx entry secure_boot_info=0x%08lx\r\n", SAU->CTRL, (uint32_t)__nsc_region_start,
           (uint32_t)__nsc_region_end - 1U, (uint32_t)&secure_boot_info);
    printf("[s] ns vtor=0x%08lx sp=0x%08lx reset=0x%08lx, console to ns\r\n", (uint32_t)TZ_NS_VECTOR_TABLE,
           ((const uint32_t *)TZ_NS_VECTOR_TABLE)[0], ((const uint32_t *)TZ_NS_VECTOR_TABLE)[1]);

    ret = board_console_hand_to_nonsecure();
    if (ret != 0) {
        halt();
    }

    ret = tz_partition_start_nonsecure(TZ_NS_VECTOR_TABLE);

    /* The console belongs to the Non-secure side now; the LED blink is the only report left. */
    (void)ret;
    halt();
}

/* Leaves SFSR and SFAR as the fault set them, for SWD (E10.3). */
void SecureFault_Handler(void)
{
    while (1) {
    }
}
