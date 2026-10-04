#ifndef MCUBOOT_CONFIG_H
#define MCUBOOT_CONFIG_H

#include "flash_layout.h"

#define MCUBOOT_SIGN_EC256
#define MCUBOOT_USE_MBED_TLS
#define MCUBOOT_IMAGE_NUMBER 1
#define MCUBOOT_OVERWRITE_ONLY
#define MCUBOOT_VALIDATE_PRIMARY_SLOT
#define MCUBOOT_BOOT_MAX_ALIGN 16
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS
#define MCUBOOT_MAX_IMG_SECTORS (FLASH_AREA_0_SIZE / FLASH_AREA_IMAGE_SECTOR_SIZE)

#define MCUBOOT_HAVE_LOGGING    1
#define MCUBOOT_HAVE_ASSERT_H   1

#define MCUBOOT_CPU_IDLE()                                                                                                                           \
    do {                                                                                                                                             \
    } while (0)

/* The watchdog is not started in M2; M8 brings it back. */
#define MCUBOOT_WATCHDOG_FEED()                                                                                                                      \
    do {                                                                                                                                             \
    } while (0)

#endif /* MCUBOOT_CONFIG_H */
