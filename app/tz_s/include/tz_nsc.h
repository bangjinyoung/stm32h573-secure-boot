#ifndef TZ_NSC_H
#define TZ_NSC_H

#include <stdint.h>

/*
 * The Secure app's only entry for the Non-secure app (D-29). Bits 7:0 hold SBS HDPLSR.HDPL, bits 15:8 and 23:16 the
 * major and minor version from the Secure image header.
 */
uint32_t secure_boot_info(void);

#define TZ_NSC_INFO_HDPL(info)  ((info) & 0xFFU)
#define TZ_NSC_INFO_MAJOR(info) (((info) >> 8) & 0xFFU)
#define TZ_NSC_INFO_MINOR(info) (((info) >> 16) & 0xFFU)

#endif /* TZ_NSC_H */
