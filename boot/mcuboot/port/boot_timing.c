#include "boot_timing.h"

#include <stdio.h>

#include "bootutil/fault_injection_hardening.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"
#include "stm32h5xx_hal.h"

#define BOOT_TIMING_MAX_CHECKS 4U

struct boot_timing_check {
    uint8_t area_id;
    int32_t rc;
    uint32_t validate;
    uint32_t hash;
    uint32_t sig;
};

static struct boot_timing_check checks[BOOT_TIMING_MAX_CHECKS];
static uint32_t check_count;
static struct boot_timing_check *current;
static uint32_t boot_go_start;

fih_ret __real_bootutil_img_validate(struct boot_loader_state *state, struct image_header *hdr, const struct flash_area *fap,
                                     uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *seed, int seed_len, uint8_t *out_hash);
int __real_bootutil_img_hash(struct boot_loader_state *state, struct image_header *hdr, const struct flash_area *fap,
                             uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *hash_result, uint8_t *seed, int seed_len);
fih_ret __real_bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen, uint8_t key_id);

fih_ret __wrap_bootutil_img_validate(struct boot_loader_state *state, struct image_header *hdr, const struct flash_area *fap,
                                     uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *seed, int seed_len, uint8_t *out_hash)
{
    struct boot_timing_check *check = NULL;
    uint32_t start;
    fih_ret rc;

    if (check_count < BOOT_TIMING_MAX_CHECKS) {
        check = &checks[check_count++];
        check->area_id = flash_area_get_id(fap);
    }
    current = check;

    start = DWT->CYCCNT;
    rc = __real_bootutil_img_validate(state, hdr, fap, tmp_buf, tmp_buf_sz, seed, seed_len, out_hash);
    if (check != NULL) {
        check->validate = DWT->CYCCNT - start;
        check->rc = (int32_t)rc;
    }
    current = NULL;

    return rc;
}

int __wrap_bootutil_img_hash(struct boot_loader_state *state, struct image_header *hdr, const struct flash_area *fap,
                             uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *hash_result, uint8_t *seed, int seed_len)
{
    uint32_t start = DWT->CYCCNT;
    int rc = __real_bootutil_img_hash(state, hdr, fap, tmp_buf, tmp_buf_sz, hash_result, seed, seed_len);

    if (current != NULL) {
        current->hash += DWT->CYCCNT - start;
    }
    return rc;
}

fih_ret __wrap_bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen, uint8_t key_id)
{
    uint32_t start = DWT->CYCCNT;
    fih_ret rc = __real_bootutil_verify_sig(hash, hlen, sig, slen, key_id);

    if (current != NULL) {
        current->sig += DWT->CYCCNT - start;
    }
    return rc;
}

void boot_timing_start(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    boot_go_start = DWT->CYCCNT;
}

void boot_timing_report(void)
{
    uint32_t boot_go = DWT->CYCCNT - boot_go_start;
    uint32_t mhz = SystemCoreClock / 1000000U;
    uint32_t i;

    printf("[bl2] timing sysclk=%lu hz boot_go=%lu cycles (%lu us)\r\n", SystemCoreClock, boot_go, boot_go / mhz);
    for (i = 0U; i < check_count; i++) {
        printf("[bl2] timing area=%u rc=%ld validate=%lu hash=%lu sig=%lu cycles (%lu/%lu/%lu us)\r\n", checks[i].area_id,
               checks[i].rc, checks[i].validate, checks[i].hash, checks[i].sig, checks[i].validate / mhz,
               checks[i].hash / mhz, checks[i].sig / mhz);
    }
}
