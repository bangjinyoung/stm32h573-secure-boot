#include <stdio.h>

#include "build_id.h"

#include "board.h"
#include "stm32h5xx.h"

#define LED_PERIOD_MS 500U

/* BL2's rollback record in the HDPL2 option-byte keys (boot_rollback.c). */
#define ROLLBACK_OBK_ADDR (FLASH_OBK_BASE_S + 0xB10U)

static void print_status(void)
{
    struct board_boot_info boot_info;

    board_get_boot_info(&boot_info);
    printf("[bare] status uptime_ms=%lu hdpl=0x%02lx vtor=0x%08lx mpu_ctrl=0x%08lx\r\n", board_uptime_ms(), boot_info.hdpl, boot_info.vtor,
           boot_info.mpu_ctrl);
}

/*
 * The app runs at HDPL3, so the HDPL2 keys must stay out of reach (RM0481: OBK-HDPL has to match
 * the key area). Prints what a read returns and the flash error flags around it.
 */
static void probe_rollback_obk(void)
{
    const volatile uint32_t *record = (const volatile uint32_t *)ROLLBACK_OBK_ADDR;
    uint32_t word0;
    uint32_t word1;

    printf("[bare] obk probe addr=0x%08lx secsr=0x%08lx\r\n", (uint32_t)ROLLBACK_OBK_ADDR, FLASH->SECSR);
    word0 = record[0];
    word1 = record[1];
    printf("[bare] obk probe read=%08lx %08lx secsr=0x%08lx\r\n", word0, word1, FLASH->SECSR);
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
    printf("[bare] running; press o to probe the hdpl2 obk, any other key for a status line\r\n");

    /* The LED shows the app is alive; the console stays quiet until asked. */
    led_time = board_uptime_ms();
    while (1) {
        if (board_console_getc(&c) == 0) {
            if (c == 'o') {
                probe_rollback_obk();
            } else {
                print_status();
            }
        }

        if ((board_uptime_ms() - led_time) >= LED_PERIOD_MS) {
            led_time += LED_PERIOD_MS;
            board_led_toggle();
        }
    }
}
