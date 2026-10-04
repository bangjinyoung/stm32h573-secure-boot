/*
 * mbedtls_boot_config.h - verify-only Mbed TLS configuration for the
 * STM32H563 bootloader.
 *
 * Scope: only the primitives mcuboot's image validation calls when
 * MCUBOOT_SIGN_EC256 + MCUBOOT_USE_MBED_TLS are on:
 *   - SHA-256 hashing
 *   - ECDSA-P256 signature verification
 *   - ASN.1/OID parsing for signature + key structures
 *
 * Intentionally NOT enabled: entropy sources, DRBG, AES, cipher modes,
 * ALT hardware backends, platform_* redirects. Those pull in ST-side
 * hooks (mbedtls_hardware_poll, SAES/HASH glue, etc.) that this
 * project does not carry.
 */

#ifndef MBEDTLS_CONFIG_BOOT_H
#define MBEDTLS_CONFIG_BOOT_H

/* Toolchain / general.
 * MBEDTLS_HAVE_ASM is intentionally NOT defined: mbedtls's Cortex-M
 * inline-asm paths (bn_mul.h MULADDC_X*_INIT) cannot allocate registers
 * at -O0 and break the Debug build. The pure-C fallback is fast enough
 * for signature verification in a bootloader.
 */
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

/* Platform: use libc directly (no *_ALT overrides). */
#define MBEDTLS_PLATFORM_C

/* Bignum + elliptic curve arithmetic */
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM

/* ASN.1 (signature + subjectPublicKeyInfo parsing) */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C

/* Message-digest abstraction + SHA-256 */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C

/* ECDSA verify */
#define MBEDTLS_ECDSA_C

#endif /* MBEDTLS_CONFIG_BOOT_H */
