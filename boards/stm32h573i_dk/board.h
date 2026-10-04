#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

struct board_boot_info {
    uint32_t vtor;
    uint32_t initial_sp;
    uint32_t reset_handler;
    uint32_t reset_flags;
    uint32_t core_clock_hz;
};

int board_init(void);
void board_get_boot_info(struct board_boot_info *info);
void board_led_toggle(void);
void board_delay_ms(uint32_t delay_ms);

#endif /* BOARD_H */
