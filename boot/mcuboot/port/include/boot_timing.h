#ifndef BOOT_TIMING_H
#define BOOT_TIMING_H

#include <stdint.h>

/*
 * DWT cycle counts of MCUboot's image checks, built with BL2_TIMING. The linker wraps
 * bootutil_img_validate, bootutil_img_hash and bootutil_verify_sig, so MCUboot itself is unchanged.
 * Call boot_timing_start() right before boot_go() and boot_timing_report() after it.
 */
void boot_timing_start(void);
void boot_timing_report(void);

#endif /* BOOT_TIMING_H */
