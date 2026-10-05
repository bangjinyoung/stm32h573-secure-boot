/*
 * flash_map_backend.c - MCUboot flash abstraction for STM32H563.
 *
 * Adapter between mcuboot bootutil (which speaks the flash_area_* API)
 * and the ARM CMSIS Flash Driver instance Driver_FLASH0 provided by
 * port/stm32h5/src/low_level_flash.c.
 *
 * Scope:
 *   - Primary and secondary slots of one signed application image.
 *   - Internal Flash device only.
 *   - Encryption and hardware rollback protection are not configured.
 *
 * Write and erase are forwarded to Driver_FLASH0.ProgramData /
 * .EraseSector for any area that IS in the table, so an eventual
 * mcuboot code path that touches the primary slot works end-to-end
 * instead of silently succeeding or hard-failing on an unsupported
 * error.
 *
 * Address contract with Driver_FLASH0:
 *   Driver_FLASH0.ReadData(addr, buf, cnt) treats 'addr' as a byte
 *   offset from the start of flash. low_level_flash.c:Flash_ReadData
 *   adds FLASH_BASE (Secure alias) inside the Secure partition and
 *   FLASH_BASE_NS outside it.
 *
 *   flash_area.fa_off carries the same semantics: it is an offset from
 *   FLASH_BASE, not an absolute CPU address. flash_area_read therefore
 *   forwards 'fa->fa_off + off' directly. Do NOT add FLASH_BASE or
 *   FLASH_BASE_ADDRESS here or the driver will double-shift into the
 *   wrong memory.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * flash_layout.h references FLASH_DEVICE_ID inside the ST-provided
 * FLASH_AREAS_DEVICE_ID / FLASH_AREA_x_DEVICE_ID formulas. In the
 * upstream mcuboot tree this symbol lives in a bl2 header we do not
 * import; define it locally so the ST layout math compiles unchanged.
 * The value is only used as (FLASH_DEVICE_ID - FLASH_DEVICE_ID) == 0,
 * so any value is fine.
 */
#ifndef FLASH_DEVICE_ID
#define FLASH_DEVICE_ID  100
#endif

#include "flash_layout.h"
#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "Driver_Flash.h"

/* Driver_FLASH0 is defined in port/stm32h5/src/low_level_flash.c. */
extern ARM_DRIVER_FLASH Driver_FLASH0;

/*
 * Local return codes. Kept file-local to avoid dragging in
 * libc <errno.h> semantics that vary between newlib builds. mcuboot
 * bootutil only checks 'rc != 0' for flash_area_* results, so any
 * negative value is treated as failure.
 */
enum {
    FMB_OK          =  0,
    FMB_ERR_INVAL   = -1,   /* NULL argument or bad alignment */
    FMB_ERR_NOTFND  = -2,   /* area id not registered */
    FMB_ERR_RANGE   = -3,   /* offset/length outside the area */
    FMB_ERR_NOBUF   = -4,   /* caller-provided array too small */
    FMB_ERR_UNSUP   = -5,   /* operation not supported in this phase */
};

/*
 * Static flash-area table for both slots of the single application
 * image. The offsets/sizes come from
 * config/flash_layout.h, which is the single source of truth for
 * the on-flash layout.
 *
 * FLASH_AREA_0 is the active primary slot. FLASH_AREA_1 is the staging
 * slot used by MCUboot's overwrite-only update path.
 */
static const struct flash_area s_flash_map[] = {
    {
        .fa_id        = FLASH_AREA_IMAGE_0,        /* sysflash.h: 1 */
        .fa_device_id = FLASH_AREA_0_DEVICE_ID,
        .pad16        = 0,
        .fa_off       = FLASH_AREA_0_OFFSET,
        .fa_size      = FLASH_AREA_0_SIZE,
    },
    {
        .fa_id        = FLASH_AREA_IMAGE_1,        /* sysflash.h: 2 */
        .fa_device_id = FLASH_AREA_1_DEVICE_ID,
        .pad16        = 0,
        .fa_off       = FLASH_AREA_1_OFFSET,
        .fa_size      = FLASH_AREA_1_SIZE,
    },
};

static const size_t s_flash_map_len =
    sizeof(s_flash_map) / sizeof(s_flash_map[0]);

static const struct flash_area *find_area_by_id(uint8_t id)
{
    for (size_t i = 0; i < s_flash_map_len; ++i) {
        if (s_flash_map[i].fa_id == id) {
            return &s_flash_map[i];
        }
    }
    return NULL;
}

/* Bounds + overflow guard for [off, off+len) inside a flash_area. */
static bool range_within_area(const struct flash_area *fap,
                              uint32_t off, uint32_t len)
{
    if (fap == NULL || len == 0U) {
        return false;
    }
    if (off > fap->fa_size) {
        return false;
    }
    /* overflow check for (off + len) */
    if (len > (UINT32_MAX - off)) {
        return false;
    }
    if ((off + len) > fap->fa_size) {
        return false;
    }
    return true;
}

int flash_area_open(uint8_t id, const struct flash_area **fap)
{
    if (fap == NULL) {
        return FMB_ERR_INVAL;
    }
    const struct flash_area *fa = find_area_by_id(id);
    if (fa == NULL) {
        *fap = NULL;
        return FMB_ERR_NOTFND;
    }
    *fap = fa;
    return FMB_OK;
}

void flash_area_close(const struct flash_area *fap)
{
    /* Static table entries; no per-open state to release. */
    (void)fap;
}

int flash_area_read(const struct flash_area *fap, uint32_t off,
                    void *dst, uint32_t len)
{
    if (dst == NULL) {
        return FMB_ERR_INVAL;
    }
    if (!range_within_area(fap, off, len)) {
        return FMB_ERR_RANGE;
    }
    /* fa_off + off is already an offset from FLASH_BASE. */
    return Driver_FLASH0.ReadData(fap->fa_off + off, dst, len);
}

int flash_area_write(const struct flash_area *fap, uint32_t off,
                     const void *src, uint32_t len)
{
    if (src == NULL) {
        return FMB_ERR_INVAL;
    }
    if (!range_within_area(fap, off, len)) {
        return FMB_ERR_RANGE;
    }
    /*
     * Driver_FLASH0.ProgramData enforces program_unit alignment on both
     * address and length internally and returns ARM_DRIVER_ERROR_PARAMETER
     * for a bad request. Same address contract as flash_area_read:
     * fa_off + off is FLASH_BASE-relative.
     */
    return Driver_FLASH0.ProgramData(fap->fa_off + off, src, len);
}

int flash_area_erase(const struct flash_area *fap, uint32_t off, uint32_t len)
{
    if (!range_within_area(fap, off, len)) {
        return FMB_ERR_RANGE;
    }

    ARM_FLASH_INFO *info = Driver_FLASH0.GetInfo();
    if (info == NULL || info->sector_size == 0U) {
        return FMB_ERR_INVAL;
    }
    if (info->sector_info != NULL) {
        /* Non-uniform sector layouts (per-sector info array) are not
         * handled at this stage. STM32H5 internal flash reports NULL
         * here (uniform 8 KB sectors), so this branch is defensive. */
        return FMB_ERR_UNSUP;
    }

    /* Erase must be sector-aligned on both ends; catching this here is
     * cheaper than letting the HAL fault deep inside the driver. */
    if ((off % info->sector_size) != 0U ||
        (len % info->sector_size) != 0U) {
        return FMB_ERR_INVAL;
    }

    uint32_t remaining = len;
    uint32_t cur_off   = off;
    while (remaining > 0U) {
        int32_t rc = Driver_FLASH0.EraseSector(fap->fa_off + cur_off);
        if (rc != 0) {
            return rc;
        }
        cur_off   += info->sector_size;
        remaining -= info->sector_size;
    }
    return FMB_OK;
}

uint32_t flash_area_align(const struct flash_area *fap)
{
    (void)fap;
    ARM_FLASH_INFO *info = Driver_FLASH0.GetInfo();
    if (info == NULL || info->program_unit == 0U) {
        /* Should never happen with the STM32H5 driver, but keep the
         * caller from dividing by zero if it ever does.
         */
        return 1U;
    }
    return info->program_unit;
}

uint8_t flash_area_erased_val(const struct flash_area *fap)
{
    (void)fap;
    ARM_FLASH_INFO *info = Driver_FLASH0.GetInfo();
    if (info == NULL) {
        return 0xFFU;
    }
    return info->erased_value;
}

int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors)
{
    if (count == NULL || sectors == NULL) {
        return FMB_ERR_INVAL;
    }
    if (fa_id < 0 || fa_id > 0xFF) {
        return FMB_ERR_INVAL;
    }

    const struct flash_area *fa = find_area_by_id((uint8_t)fa_id);
    if (fa == NULL) {
        return FMB_ERR_NOTFND;
    }

    /* STM32H5 internal flash: uniform 8 KB sectors. */
    const uint32_t sector_size = FLASH_AREA_IMAGE_SECTOR_SIZE;
    if (sector_size == 0U || (fa->fa_size % sector_size) != 0U) {
        return FMB_ERR_INVAL;
    }

    const uint32_t need = fa->fa_size / sector_size;
    if (need > *count) {
        /* Tell caller the real requirement so it can size the array. */
        *count = need;
        return FMB_ERR_NOBUF;
    }

    for (uint32_t i = 0; i < need; ++i) {
        sectors[i].fs_off  = i * sector_size;
        sectors[i].fs_size = sector_size;
    }
    *count = need;
    return FMB_OK;
}

int flash_area_get_sector(const struct flash_area *fap, uint32_t off,
                          struct flash_sector *fs)
{
    if (fap == NULL || fs == NULL) {
        return FMB_ERR_INVAL;
    }
    if (off >= fap->fa_size) {
        return FMB_ERR_RANGE;
    }

    const uint32_t sector_size = FLASH_AREA_IMAGE_SECTOR_SIZE;
    if (sector_size == 0U) {
        return FMB_ERR_INVAL;
    }

    const uint32_t idx = off / sector_size;
    fs->fs_off  = idx * sector_size;
    fs->fs_size = sector_size;
    return FMB_OK;
}

int flash_area_id_from_image_slot(int slot)
{
    if (slot == 0) {
        return FLASH_AREA_IMAGE_0;
    }
    if (slot == 1) {
        return FLASH_AREA_IMAGE_1;
    }
    return -1;
}

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    if (image_index != 0) {
        return -1;
    }
    return flash_area_id_from_image_slot(slot);
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if (image_index != 0) {
        return -1;
    }
    if (area_id == FLASH_AREA_IMAGE_0) {
        return 0;
    }
    if (area_id == FLASH_AREA_IMAGE_1) {
        return 1;
    }
    return -1;
}
