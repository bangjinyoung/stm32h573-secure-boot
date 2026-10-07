#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

/*
 * Offsets are relative to FLASH_BASE_ADDRESS. BL2 occupies the HDPL2 code area STiRoT installs
 * (STiRoT_Config: 0x0, 0x18000). Image 0 is the Secure app, image 1 the Non-secure app, as in
 * ST's OEMiROT TrustZone layout (MCUBOOT_APP_IMAGE_NUMBER 2). Only the image 0 primary slot is
 * Secure flash (SECWM1 sectors 0-0xF); every other slot is Non-secure flash.
 */
#define FLASH_BASE_ADDRESS           0x0C000000U
#define FLASH_BASE_ADDRESS_NS        0x08000000U
#define FLASH_TOTAL_SIZE             0x00200000U
#define FLASH_AREA_IMAGE_SECTOR_SIZE 0x00002000U
#define FLASH_SECURE_AREA_END        0x00020000U

#define FLASH_AREA_BL2_OFFSET        0x00000000U
#define FLASH_AREA_BL2_SIZE          0x00018000U

#define BL2_IMAGE_HEADER_SIZE        0x00000400U

#define FLASH_S_PARTITION_SIZE       0x00008000U
#define FLASH_NS_PARTITION_SIZE      0x00010000U
#define FLASH_MAX_PARTITION_SIZE     FLASH_NS_PARTITION_SIZE

/* Image 0 (Secure app) primary slot. The flash driver reaches it through the Secure alias. */
#define FLASH_AREA_0_DEVICE_ID       0U
#define FLASH_AREA_0_OFFSET          (FLASH_AREA_BL2_OFFSET + FLASH_AREA_BL2_SIZE)
#define FLASH_AREA_0_SIZE            FLASH_S_PARTITION_SIZE

/* Image 1 (Non-secure app) primary slot. */
#define FLASH_AREA_1_DEVICE_ID       0U
#define FLASH_AREA_1_OFFSET          (FLASH_AREA_0_OFFSET + FLASH_AREA_0_SIZE)
#define FLASH_AREA_1_SIZE            FLASH_NS_PARTITION_SIZE

/* Image 0 secondary slot. */
#define FLASH_AREA_2_DEVICE_ID       0U
#define FLASH_AREA_2_OFFSET          (FLASH_AREA_1_OFFSET + FLASH_AREA_1_SIZE)
#define FLASH_AREA_2_SIZE            FLASH_S_PARTITION_SIZE

/* Image 1 secondary slot. */
#define FLASH_AREA_3_DEVICE_ID       0U
#define FLASH_AREA_3_OFFSET          (FLASH_AREA_2_OFFSET + FLASH_AREA_2_SIZE)
#define FLASH_AREA_3_SIZE            FLASH_NS_PARTITION_SIZE

#define FLASH_AREA_BEGIN_OFFSET      FLASH_AREA_0_OFFSET
#define FLASH_AREA_END_OFFSET        (FLASH_AREA_3_OFFSET + FLASH_AREA_3_SIZE)

#if (FLASH_AREA_BL2_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_BL2_SIZE not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif

#if ((FLASH_S_PARTITION_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0) || ((FLASH_NS_PARTITION_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0)
#error "Slot sizes not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif

#if (FLASH_AREA_0_OFFSET + FLASH_AREA_0_SIZE) > FLASH_SECURE_AREA_END
#error "Primary slot extends past the Secure watermark SECWM1 0-0xF"
#endif

#if FLASH_AREA_1_OFFSET < FLASH_SECURE_AREA_END
#error "Non-secure slots overlap the Secure watermark; the driver reaches them through the Non-secure alias"
#endif

#if FLASH_AREA_END_OFFSET > 0x001E6000U
#error "Application slots overlap the STiRoT download areas"
#endif

#endif /* FLASH_LAYOUT_H */
