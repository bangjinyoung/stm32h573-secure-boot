/*
 * mcuboot_logging.h - printf-routed logging for bl2.
 *
 * upstream bootutil_log.h maps BOOT_LOG_* to MCUBOOT_LOG_* whenever
 * MCUBOOT_HAVE_LOGGING is defined (see mcuboot_config.h). The leaf
 * macros below emit through mcuboot_log_printf(), which forwards to the
 * C library printf already wired to COM1 (115200-8N1) by Core/Src/main.c
 * via BSP_COM_Init.
 *
 * MCUBOOT_LOG_LEVEL selects the severity threshold. DEBUG is the default:
 * upstream reports which TLV check an image failed (hash or signature)
 * only at DEBUG, and the M8 negative tests tell the two apart by it.
 */

#ifndef MCUBOOT_LOGGING_H
#define MCUBOOT_LOGGING_H

#include <stdio.h>

void mcuboot_log_printf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

#define MCUBOOT_LOG_LEVEL_OFF     0
#define MCUBOOT_LOG_LEVEL_ERROR   1
#define MCUBOOT_LOG_LEVEL_WARNING 2
#define MCUBOOT_LOG_LEVEL_INFO    3
#define MCUBOOT_LOG_LEVEL_DEBUG   4

#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_DEBUG
#endif

#define MCUBOOT_LOG_MODULE_DECLARE(domain)  /* no-op */
#define MCUBOOT_LOG_MODULE_REGISTER(domain) /* no-op */

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_ERROR
#define MCUBOOT_LOG_ERR(fmt, ...) \
    mcuboot_log_printf("[mcuboot ERR] " fmt "\r\n", ##__VA_ARGS__)
#else
#define MCUBOOT_LOG_ERR(...) do { } while (0)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_WARNING
#define MCUBOOT_LOG_WRN(fmt, ...) \
    mcuboot_log_printf("[mcuboot WRN] " fmt "\r\n", ##__VA_ARGS__)
#else
#define MCUBOOT_LOG_WRN(...) do { } while (0)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_INFO
#define MCUBOOT_LOG_INF(fmt, ...) \
    mcuboot_log_printf("[mcuboot INF] " fmt "\r\n", ##__VA_ARGS__)
#else
#define MCUBOOT_LOG_INF(...) do { } while (0)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_DEBUG
#define MCUBOOT_LOG_DBG(fmt, ...) \
    mcuboot_log_printf("[mcuboot DBG] " fmt "\r\n", ##__VA_ARGS__)
#else
#define MCUBOOT_LOG_DBG(...) do { } while (0)
#endif

/* Simulator-only sink used by upstream unit tests; no-op on target. */
#define MCUBOOT_LOG_SIM(...) do { } while (0)

#endif /* MCUBOOT_LOGGING_H */
