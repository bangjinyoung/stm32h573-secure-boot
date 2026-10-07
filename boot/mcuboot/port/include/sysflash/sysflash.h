/*
 * sysflash/sysflash.h - flash-area IDs for the STM32H5 OEMiROT slot layout.
 *
 * Image 0 is the Secure app, image 1 the Non-secure app; each has a primary
 * and a secondary slot. Numeric IDs are opaque handles passed to
 * flash_area_open() and matched inside the port's flash_map_backend.
 */

#ifndef SYSFLASH_SYSFLASH_H
#define SYSFLASH_SYSFLASH_H


#define FLASH_AREA_BL2              0
#define FLASH_AREA_0_ID             1
#define FLASH_AREA_1_ID             2
#define FLASH_AREA_2_ID             3
#define FLASH_AREA_3_ID             4
/* Overwrite-only has no scratch area; the id matches no table entry. */
#define FLASH_AREA_IMAGE_SCRATCH    5
#define FLASH_AREA_ID_NONE          255

#define FLASH_AREA_IMAGE_PRIMARY(x)   (((x) == 0) ? FLASH_AREA_0_ID : ((x) == 1) ? FLASH_AREA_1_ID : FLASH_AREA_ID_NONE)
#define FLASH_AREA_IMAGE_SECONDARY(x) (((x) == 0) ? FLASH_AREA_2_ID : ((x) == 1) ? FLASH_AREA_3_ID : FLASH_AREA_ID_NONE)

#endif /* SYSFLASH_SYSFLASH_H */
