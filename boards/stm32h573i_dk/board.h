#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

struct board_boot_info {
    uint32_t vtor;
    uint32_t initial_sp;
    uint32_t reset_handler;
    uint32_t reset_flags;
    uint32_t core_clock_hz;
    uint32_t hdpl;
    uint32_t mpu_ctrl;
};

int board_init(void);
void board_get_boot_info(struct board_boot_info *info);
void board_led_toggle(void);
void board_delay_ms(uint32_t delay_ms);
uint32_t board_uptime_ms(void);
/* Returns 0 with one received console character, or -EAGAIN at once when none is waiting. */
int board_console_getc(char *c);
/*
 * Waits for the last console byte to leave, then makes USART1 and its two pins Non-secure. USART1 keeps its
 * configuration; the Secure side must not use the console afterwards.
 */
int board_console_hand_to_nonsecure(void);

#endif /* BOARD_H */
