/*
 * sysflash/sysflash.h - flash-area IDs for the STM32H5 OEMiROT slot layout.
 *
 * Single application image: slot 0 (primary), slot 1 (secondary).
 * Numeric IDs are opaque handles passed to flash_area_open() and matched
 * inside the port's flash_map_backend implementation.
 */

#ifndef SYSFLASH_SYSFLASH_H
#define SYSFLASH_SYSFLASH_H


#define FLASH_AREA_BL2              0
#define FLASH_AREA_IMAGE_0          1
#define FLASH_AREA_IMAGE_1          2
#define FLASH_AREA_IMAGE_SCRATCH    3

/*
 * Only image_index == 0 is supported. The argument is intentionally unused
 * because this project has one image with primary and secondary slots.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)     ((void)(x), FLASH_AREA_IMAGE_0)
#define FLASH_AREA_IMAGE_SECONDARY(x)   ((void)(x), FLASH_AREA_IMAGE_1)

#endif /* SYSFLASH_SYSFLASH_H */
