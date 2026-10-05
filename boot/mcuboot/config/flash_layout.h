#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

/*
 * Offsets are relative to FLASH_BASE_ADDRESS. BL2 occupies the HDPL2 code area STiRoT installs
 * (STiRoT_Config: 0x0, 0x18000). The primary slot is Secure flash (SECWM1 sectors 0-0xF) and
 * the secondary slot is Non-secure flash, as in ST's full-secure OEMuRoT layout.
 */
#define FLASH_BASE_ADDRESS           0x0C000000U
#define FLASH_BASE_ADDRESS_NS        0x08000000U
#define FLASH_TOTAL_SIZE             0x00200000U
#define FLASH_AREA_IMAGE_SECTOR_SIZE 0x00002000U
#define FLASH_SECURE_AREA_END        0x00020000U

#define FLASH_AREA_BL2_OFFSET        0x00000000U
#define FLASH_AREA_BL2_SIZE          0x00018000U

#define BL2_IMAGE_HEADER_SIZE        0x00000400U

#define FLASH_AREA_0_ID              1U
#define FLASH_AREA_0_DEVICE_ID       0U
#define FLASH_AREA_0_OFFSET          (FLASH_AREA_BL2_OFFSET + FLASH_AREA_BL2_SIZE)
#define FLASH_AREA_0_SIZE            0x00008000U

#define FLASH_AREA_1_ID              2U
#define FLASH_AREA_1_DEVICE_ID       0U
#define FLASH_AREA_1_OFFSET          (FLASH_AREA_0_OFFSET + FLASH_AREA_0_SIZE)
#define FLASH_AREA_1_SIZE            FLASH_AREA_0_SIZE

#define FLASH_AREA_BEGIN_OFFSET      FLASH_AREA_0_OFFSET
#define FLASH_AREA_END_OFFSET        (FLASH_AREA_1_OFFSET + FLASH_AREA_1_SIZE)

/* The flash driver accesses this part of the slots through the Secure alias and SECCR. */
#define FLASH_S_PARTITION_SIZE       FLASH_AREA_0_SIZE

#if (FLASH_AREA_BL2_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_BL2_SIZE not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif

#if (FLASH_AREA_0_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_0_SIZE not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif

#if (FLASH_AREA_0_OFFSET + FLASH_AREA_0_SIZE) > FLASH_SECURE_AREA_END
#error "Primary slot extends past the Secure watermark SECWM1 0-0xF"
#endif

#if FLASH_AREA_1_OFFSET < FLASH_SECURE_AREA_END
#error "Secondary slot overlaps the Secure watermark; the driver reaches it through the Non-secure alias"
#endif

#if FLASH_AREA_END_OFFSET > 0x001E6000U
#error "Application slots overlap the STiRoT download areas"
#endif

#endif /* FLASH_LAYOUT_H */
