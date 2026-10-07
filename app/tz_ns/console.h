#ifndef CONSOLE_H
#define CONSOLE_H

/*
 * USART1 as the Secure app left it (115200 8N1, enabled), driven by register polling: the Non-secure side has no
 * HAL and no clock control. printf goes through _write.
 */

/* Returns 1 with one received character in *c, 0 when none is waiting. */
int console_getc(char *c);

#endif /* CONSOLE_H */
