#include <stdio.h>

#include "build_id.h"

#include "board.h"

int main(void)
{
    struct board_boot_info boot_info;

    if (board_init() != 0) {
        while (1) {
        }
    }

    board_get_boot_info(&boot_info);
    printf("\r\n[s] stm32h573 secure app build=%s\r\n", BUILD_ID);
    printf("[s] hdpl=0x%02lx vtor=0x%08lx\r\n", boot_info.hdpl, boot_info.vtor);

    while (1) {
    }
}
