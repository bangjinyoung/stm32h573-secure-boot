/*
 * mcuboot_logging.c - printf sink for the MCUBOOT_LOG_* macros.
 *
 * newlib-nano printf does not implement the C99 'z' length modifier, so
 * upstream formats such as "0x%zx" print literally. Full newlib printf
 * does not fit in the 96 KB BL2 region. size_t is unsigned int on this
 * target, so dropping 'z' keeps the argument width unchanged.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "mcuboot_config/mcuboot_logging.h"

_Static_assert(sizeof(size_t) == sizeof(unsigned int),
               "dropping %z requires size_t to match unsigned int");

#define MCUBOOT_LOG_FMT_MAX 192U

void mcuboot_log_printf(const char *fmt, ...)
{
    char nano_fmt[MCUBOOT_LOG_FMT_MAX];
    const char *out_fmt = nano_fmt;
    size_t len = 0U;
    bool in_spec = false;
    va_list args;

    for (const char *p = fmt; *p != '\0'; p++) {
        if (in_spec && (*p == 'z')) {
            continue;
        }
        if (len == (sizeof(nano_fmt) - 1U)) {
            out_fmt = fmt;
            break;
        }
        nano_fmt[len++] = *p;
        if (!in_spec) {
            in_spec = (*p == '%');
        } else if (strchr("diouxXcspn%", *p) != NULL) {
            in_spec = false;
        }
    }
    nano_fmt[len] = '\0';

    va_start(args, fmt);
    (void)vprintf(out_fmt, args);
    va_end(args);
}
