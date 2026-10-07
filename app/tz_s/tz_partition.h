#ifndef TZ_PARTITION_H
#define TZ_PARTITION_H

#include <stdint.h>

/* Linker script (stm32h573_executable NSC): the region the cmse_nonsecure_entry veneers live in. */
extern const uint8_t __nsc_region_start[];
extern const uint8_t __nsc_region_end[];

/*
 * Sets the TrustZone partition the Non-secure app runs in: SAU regions, SRAM3 Non-secure in GTZC MPCBB3, Non-secure
 * FPU access, and SecureFault enabled. Returns 0, or a negative errno when GTZC refuses a setting; the partition is
 * then incomplete and the caller must not start the Non-secure app.
 */
int tz_partition_init(void);

/* Starts the Non-secure app from its vector table. Returns only when the vector table fails the checks. */
int tz_partition_start_nonsecure(uint32_t vector_table);

#endif /* TZ_PARTITION_H */
