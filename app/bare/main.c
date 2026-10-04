#include <stdio.h>

#include "build_id.h"

#include "board.h"

#define HEARTBEAT_PERIOD_MS 1000U

int main(void)
{
    struct board_boot_info boot_info;
    uint32_t heartbeat_count = 0;

    if (board_init() != 0) {
        while (1) {
        }
    }

    board_get_boot_info(&boot_info);

    printf("\r\n[bare] stm32h573 bare app build=%s\r\n", BUILD_ID);
    printf("[bare] vtor=0x%08lx sp=0x%08lx reset_handler=0x%08lx\r\n", boot_info.vtor, boot_info.initial_sp, boot_info.reset_handler);
    printf("[bare] reset_flags=0x%08lx core_clock_hz=%lu\r\n", boot_info.reset_flags, boot_info.core_clock_hz);

    while (1) {
        board_led_toggle();
        printf("[bare] heartbeat %lu\r\n", heartbeat_count++);
        board_delay_ms(HEARTBEAT_PERIOD_MS);
    }
}
