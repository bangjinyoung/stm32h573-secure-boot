#include <stdio.h>

#include "build_id.h"

#include "board.h"

#define LED_PERIOD_MS 500U

static void print_status(void)
{
    struct board_boot_info boot_info;

    board_get_boot_info(&boot_info);
    printf("[bare] status uptime_ms=%lu hdpl=0x%02lx vtor=0x%08lx mpu_ctrl=0x%08lx\r\n", board_uptime_ms(), boot_info.hdpl, boot_info.vtor,
           boot_info.mpu_ctrl);
}

int main(void)
{
    struct board_boot_info boot_info;
    uint32_t led_time;
    char c;

    if (board_init() != 0) {
        while (1) {
        }
    }

    board_get_boot_info(&boot_info);

    printf("\r\n[bare] stm32h573 bare app build=%s\r\n", BUILD_ID);
    printf("[bare] vtor=0x%08lx sp=0x%08lx reset_handler=0x%08lx\r\n", boot_info.vtor, boot_info.initial_sp, boot_info.reset_handler);
    printf("[bare] reset_flags=0x%08lx core_clock_hz=%lu\r\n", boot_info.reset_flags, boot_info.core_clock_hz);
    printf("[bare] hdpl=0x%02lx mpu_ctrl=0x%08lx\r\n", boot_info.hdpl, boot_info.mpu_ctrl);
    printf("[bare] running; press any key on the console for a status line\r\n");

    /* The LED shows the app is alive; the console stays quiet until asked. */
    led_time = board_uptime_ms();
    while (1) {
        if (board_console_getc(&c) == 0) {
            print_status();
        }

        if ((board_uptime_ms() - led_time) >= LED_PERIOD_MS) {
            led_time += LED_PERIOD_MS;
            board_led_toggle();
        }
    }
}
