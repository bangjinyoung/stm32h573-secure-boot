#include "console.h"

#include "stm32h5xx.h"

static void console_putc(char c)
{
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {
    }
    USART1->TDR = (uint8_t)c;
}

int console_getc(char *c)
{
    if ((USART1->ISR & USART_ISR_RXNE_RXFNE) == 0U) {
        return 0;
    }
    *c = (char)(USART1->RDR & 0xFFU);
    return 1;
}

int _write(int file, char *ptr, int len)
{
    int i;

    (void)file;
    for (i = 0; i < len; i++) {
        console_putc(ptr[i]);
    }
    while ((USART1->ISR & USART_ISR_TC) == 0U) {
    }
    return len;
}
