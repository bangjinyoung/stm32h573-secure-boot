#include "boot_rollback.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mbedtls/sha256.h"
#include "mcuboot_config/mcuboot_config.h"
#include "stm32h5xx_hal.h"

#ifdef MCUBOOT_HW_ROLLBACK_PROT
#include "bootutil/security_cnt.h"
#endif

#define ROLLBACK_OBK_ADDR      (FLASH_OBK_BASE_S + 0xB10U)
#define ROLLBACK_MAGIC         0x4B425231U
#define ROLLBACK_FORMAT        1U
#define ROLLBACK_IMAGE_COUNT   4U
#define ROLLBACK_SHA256_SIZE   32U

/* ST OEMuRoT's HDPL2 config and data sections; BL2 only hashes them to show a swap kept them. */
#define OEMUROT_OBK_ADDR       (FLASH_OBK_BASE_S + 0x900U)
#define OEMUROT_OBK_SIZE       0x210U

/* Swap every key index: a smaller offset drops the keys above it (RM0481 OBK swap). */
#define ROLLBACK_OBK_SWAP_ALL  0x1FFU
#define ROLLBACK_OBK_PROG_UNIT 16U

struct boot_rollback_record {
    uint32_t magic;
    uint32_t format;
    uint32_t counter[ROLLBACK_IMAGE_COUNT];
    uint32_t reserved[2];
    uint8_t sha256[ROLLBACK_SHA256_SIZE];
};

_Static_assert(sizeof(struct boot_rollback_record) == 64U, "record must be four OBK quad-words");

enum boot_rollback_state {
    ROLLBACK_UNAVAILABLE,
    ROLLBACK_VIRGIN,
    ROLLBACK_VALID,
    ROLLBACK_INVALID,
};

static const char *const rollback_state_names[] = {
    [ROLLBACK_UNAVAILABLE] = "unavailable",
    [ROLLBACK_VIRGIN] = "virgin",
    [ROLLBACK_VALID] = "valid",
    [ROLLBACK_INVALID] = "invalid",
};

/* Counted by the flash driver's NMI handler on a double ECC error. */
extern __IO uint32_t DoubleECC_Error_Counter;

static enum boot_rollback_state rollback_state = ROLLBACK_UNAVAILABLE;
static struct boot_rollback_record rollback_record;

static int obk_read(uint32_t addr, void *dst, size_t len)
{
    DoubleECC_Error_Counter = 0U;
    memcpy(dst, (const void *)addr, len);
    return (DoubleECC_Error_Counter == 0U) ? 0 : -1;
}

static void rollback_digest(const struct boot_rollback_record *record, uint8_t digest[ROLLBACK_SHA256_SIZE])
{
    (void)mbedtls_sha256((const unsigned char *)record, offsetof(struct boot_rollback_record, sha256), digest, 0);
}

static int digest_equal(const uint8_t *a, const uint8_t *b)
{
    uint8_t diff = 0U;
    size_t i;

    for (i = 0U; i < ROLLBACK_SHA256_SIZE; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0U;
}

static int all_erased(const uint8_t *p, size_t len)
{
    size_t i;

    for (i = 0U; i < len; i++) {
        if (p[i] != 0xFFU) {
            return 0;
        }
    }
    return 1;
}

static enum boot_rollback_state rollback_load(struct boot_rollback_record *record)
{
    uint8_t digest[ROLLBACK_SHA256_SIZE];

    if (obk_read(ROLLBACK_OBK_ADDR, record, sizeof(*record)) != 0) {
        return ROLLBACK_UNAVAILABLE;
    }
    if (all_erased((const uint8_t *)record, sizeof(*record))) {
        return ROLLBACK_VIRGIN;
    }
    if ((record->magic != ROLLBACK_MAGIC) || (record->format != ROLLBACK_FORMAT)) {
        return ROLLBACK_INVALID;
    }
    rollback_digest(record, digest);
    return digest_equal(digest, record->sha256) ? ROLLBACK_VALID : ROLLBACK_INVALID;
}

/*
 * A reset during an OBK swap leaves the sector selectors half switched (RM0481 FLASH_OPSR).
 * ST OEMuRoT reloads them with an option byte launch at startup; BL2 does the same before it
 * reads the record. A reset during the alternate sector erase needs nothing: the current sector
 * is intact and the next counter update starts with a fresh erase. Only the BL2 that writes the
 * OBK recovers; the version-check BL2 reports the code and leaves the option bytes alone.
 */
static void rollback_recover_interrupted_swap(void)
{
    uint32_t op = READ_BIT(FLASH->OPSR, FLASH_OPSR_CODE_OP);

    if (op == FLASH_OPERATION_TYPE_NONE) {
        return;
    }
    printf("[bl2] interrupted flash op=0x%08lx\r\n", op);
#ifdef MCUBOOT_HW_ROLLBACK_PROT
    if (op != FLASH_OPERATION_TYPE_OBKSWAP) {
        return;
    }

    (void)HAL_FLASH_Unlock();
    (void)HAL_FLASH_OB_Unlock();
    if (HAL_FLASH_OB_Launch() != HAL_OK) {
        printf("[bl2] obk swap recovery failed err=0x%08lx\r\n", HAL_FLASH_GetError());
    }
    (void)HAL_FLASH_OB_Lock();
    (void)HAL_FLASH_Lock();
#endif
}

static void rollback_log(void)
{
    uint8_t oemurot[OEMUROT_OBK_SIZE];
    uint8_t digest[ROLLBACK_SHA256_SIZE];

    printf("[bl2] rollback record state=%s", rollback_state_names[rollback_state]);
    if (rollback_state == ROLLBACK_VALID) {
        printf(" counter0=%lu", rollback_record.counter[0]);
    }
    printf("\r\n");

    if (obk_read(OEMUROT_OBK_ADDR, oemurot, sizeof(oemurot)) != 0) {
        printf("[bl2] oemurot obk unreadable\r\n");
        return;
    }
    (void)mbedtls_sha256(oemurot, sizeof(oemurot), digest, 0);
    printf("[bl2] oemurot obk sha256=%02x%02x%02x%02x\r\n", digest[0], digest[1], digest[2], digest[3]);
}

void boot_rollback_init(void)
{
    rollback_recover_interrupted_swap();
    rollback_state = rollback_load(&rollback_record);
    rollback_log();
}

#ifdef MCUBOOT_HW_ROLLBACK_PROT

static int rollback_program(const struct boot_rollback_record *record)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    uint32_t offset;
    int rc = -1;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }
    if (HAL_FLASHEx_OBK_Unlock() != HAL_OK) {
        goto lock;
    }

    erase.TypeErase = FLASH_TYPEERASE_OBK_ALT;
    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK) {
        goto lock;
    }
    for (offset = 0U; offset < sizeof(*record); offset += ROLLBACK_OBK_PROG_UNIT) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD_OBK_ALT, ROLLBACK_OBK_ADDR + offset, (uint32_t)record + offset) != HAL_OK) {
            goto lock;
        }
    }
    if (HAL_FLASHEx_OBK_Swap(ROLLBACK_OBK_SWAP_ALL) != HAL_OK) {
        goto lock;
    }
    rc = 0;

lock:
    if (rc != 0) {
        printf("[bl2] rollback obk write failed err=0x%08lx secsr=0x%08lx\r\n", HAL_FLASH_GetError(), FLASH->SECSR);
    }
    (void)HAL_FLASHEx_OBK_Lock();
    (void)HAL_FLASH_Lock();
    return rc;
}

static int rollback_stored(uint32_t image_id, uint32_t *value)
{
    if (image_id >= ROLLBACK_IMAGE_COUNT) {
        return -1;
    }
    switch (rollback_state) {
    case ROLLBACK_VALID:
        *value = rollback_record.counter[image_id];
        return 0;
    case ROLLBACK_VIRGIN:
        *value = 0U;
        return 0;
    default:
        return -1;
    }
}

fih_ret boot_nv_security_counter_init(void)
{
    FIH_RET((rollback_state == ROLLBACK_VALID || rollback_state == ROLLBACK_VIRGIN) ? FIH_SUCCESS : FIH_FAILURE);
}

fih_ret boot_nv_security_counter_get(uint32_t image_id, fih_int *security_cnt)
{
    uint32_t value;

    if (rollback_stored(image_id, &value) != 0) {
        FIH_RET(FIH_FAILURE);
    }
    *security_cnt = fih_int_encode((int)value);
    FIH_RET(FIH_SUCCESS);
}

int32_t boot_nv_security_counter_update(uint32_t image_id, uint32_t img_security_cnt)
{
    struct boot_rollback_record next;
    uint32_t stored;

    if (rollback_stored(image_id, &stored) != 0) {
        return -1;
    }
    if (img_security_cnt < stored) {
        return -1;
    }
    if (img_security_cnt == stored) {
        return 0;
    }

    if (rollback_state == ROLLBACK_VALID) {
        next = rollback_record;
    } else {
        memset(&next, 0, sizeof(next));
        next.magic = ROLLBACK_MAGIC;
        next.format = ROLLBACK_FORMAT;
    }
    next.counter[image_id] = img_security_cnt;
    rollback_digest(&next, next.sha256);

    if (rollback_program(&next) != 0) {
        return -1;
    }

    rollback_state = rollback_load(&rollback_record);
    printf("[bl2] rollback counter image %lu: %lu -> %lu, record state=%s\r\n", image_id, stored, img_security_cnt,
           rollback_state_names[rollback_state]);
    rollback_log();
    if ((rollback_state != ROLLBACK_VALID) || (rollback_record.counter[image_id] != img_security_cnt)) {
        return -1;
    }
    return 0;
}

#endif /* MCUBOOT_HW_ROLLBACK_PROT */
