set(MCUBOOT_DIR ${MODULES_DIR}/mcuboot)
set(MBEDTLS_DIR ${MODULES_DIR}/mbedtls)
set(BL2_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

set(VENDOR_WARNING_OPTIONS
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-but-set-variable
    -Wno-implicit-fallthrough)

add_library(mbedcrypto STATIC
    ${MBEDTLS_DIR}/library/platform.c
    ${MBEDTLS_DIR}/library/platform_util.c
    ${MBEDTLS_DIR}/library/constant_time.c
    ${MBEDTLS_DIR}/library/bignum.c
    ${MBEDTLS_DIR}/library/bignum_core.c
    ${MBEDTLS_DIR}/library/ecp.c
    ${MBEDTLS_DIR}/library/ecp_curves.c
    ${MBEDTLS_DIR}/library/ecdsa.c
    ${MBEDTLS_DIR}/library/asn1parse.c
    ${MBEDTLS_DIR}/library/asn1write.c
    ${MBEDTLS_DIR}/library/oid.c
    ${MBEDTLS_DIR}/library/md.c
    ${MBEDTLS_DIR}/library/sha256.c)
target_include_directories(mbedcrypto PUBLIC ${MBEDTLS_DIR}/include ${BL2_CONFIG_DIR})
target_compile_definitions(mbedcrypto PUBLIC MBEDTLS_CONFIG_FILE="mbedtls_boot_config.h")
target_compile_options(mbedcrypto PRIVATE ${VENDOR_WARNING_OPTIONS})

add_library(mcuboot_bootutil STATIC
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_area.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_find_key.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_img_hash.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_img_security_cnt.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_loader.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_misc.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c
    ${MCUBOOT_DIR}/boot/bootutil/src/caps.c
    ${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening.c
    ${MCUBOOT_DIR}/boot/bootutil/src/image_ecdsa.c
    ${MCUBOOT_DIR}/boot/bootutil/src/image_validate.c
    ${MCUBOOT_DIR}/boot/bootutil/src/loader.c
    ${MCUBOOT_DIR}/boot/bootutil/src/swap_misc.c
    ${MCUBOOT_DIR}/boot/bootutil/src/swap_scratch.c
    ${MCUBOOT_DIR}/boot/bootutil/src/tlv.c)
target_include_directories(mcuboot_bootutil PUBLIC
    ${MCUBOOT_DIR}/boot/bootutil/include
    ${BL2_CONFIG_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/port/include
    ${CMAKE_CURRENT_SOURCE_DIR}/port/include/cmsis_driver)
target_include_directories(mcuboot_bootutil PRIVATE ${MCUBOOT_DIR}/boot/bootutil/src)
target_compile_options(mcuboot_bootutil PRIVATE ${VENDOR_WARNING_OPTIONS})
target_link_libraries(mcuboot_bootutil PUBLIC mbedcrypto stm32h5_hal)

set(BL2_ROLLBACK hw CACHE STRING "Rollback protection: hw (HDPL2 OBK security counter) or sw (version check only)")
set_property(CACHE BL2_ROLLBACK PROPERTY STRINGS hw sw)
if(BL2_ROLLBACK STREQUAL hw)
    target_compile_definitions(mcuboot_bootutil PUBLIC BL2_ROLLBACK_HW)
elseif(NOT BL2_ROLLBACK STREQUAL sw)
    message(FATAL_ERROR "BL2_ROLLBACK must be hw or sw, got ${BL2_ROLLBACK}")
endif()

set(BL2_PUBLIC_KEY ${CMAKE_SOURCE_DIR}/keys/mcuboot-ecdsa-p256.pub.pem)
set(BL2_KEYS_C ${CMAKE_CURRENT_BINARY_DIR}/bootutil_keys.c)
add_custom_command(
    OUTPUT ${BL2_KEYS_C}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/gen_bootutil_keys.py ${BL2_PUBLIC_KEY} ${BL2_KEYS_C}
    DEPENDS ${BL2_PUBLIC_KEY} ${CMAKE_SOURCE_DIR}/scripts/gen_bootutil_keys.py)

add_executable(bl2
    main.c
    port/boot_handoff.c
    port/boot_rollback.c
    port/boot_security.c
    port/flash_map_backend.c
    port/low_level_device.c
    port/low_level_flash.c
    port/mcuboot_logging.c
    ${BL2_KEYS_C})
target_link_libraries(bl2 PRIVATE board mcuboot_bootutil)
# MCUboot's ECDSA verification overflows the template's 1 KB stack (UsageFault STKOF on the H573).
# STiRoT installs BL2 at the start of flash behind its 0x400 image header; 92K leaves room for the TLVs in the 0x18000 area.
stm32h573_executable(bl2 0x0C000400 92K 0x2000)

# Flash driver files derived from the ST OEMiROT_Boot port keep ST's style; build them like the vendor code.
set_source_files_properties(port/flash_map_backend.c port/low_level_device.c port/low_level_flash.c
    PROPERTIES COMPILE_OPTIONS "${VENDOR_WARNING_OPTIONS};-Wno-ignored-qualifiers;-Wno-missing-field-initializers;-Wno-sign-compare")
