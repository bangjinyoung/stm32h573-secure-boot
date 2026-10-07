# Non-secure app (MCUboot image 1) in the image 1 primary slot after its header; 62K leaves room for the TLVs in the
# 0x10000 slot. SRAM3, which the Secure app makes Non-secure.
add_executable(tz_ns main.c console.c)
target_link_libraries(tz_ns PRIVATE stm32h573_device)
target_include_directories(tz_ns PRIVATE ${CMAKE_SOURCE_DIR}/app/tz_s/include ${CMAKE_CURRENT_BINARY_DIR}/generated)
stm32h573_executable(tz_ns FLASH 0x08020400 62K RAM 0x20050000 320K NONSECURE)

# The veneer address comes from the Secure link's import library, as a real Non-secure caller gets it.
add_dependencies(tz_ns tz_s)
target_link_libraries(tz_ns PRIVATE $<TARGET_PROPERTY:tz_s,STM32H573_CMSE_IMPLIB>)

# E10.3 P3 needs the address of the Secure function body behind the veneer, which the import library leaves out.
set(TZ_S_SYMBOLS_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/tz_s_symbols.h)
add_custom_command(OUTPUT ${TZ_S_SYMBOLS_HEADER}
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DELF=$<TARGET_FILE:tz_s> -DSYMBOL=__acle_se_secure_boot_info
        -DMACRO=TZ_S_SECURE_BOOT_INFO_BODY -DOUTPUT=${TZ_S_SYMBOLS_HEADER} -P ${CMAKE_SOURCE_DIR}/cmake/cmse_symbol_header.cmake
    DEPENDS tz_s ${CMAKE_SOURCE_DIR}/cmake/cmse_symbol_header.cmake)
add_custom_target(tz_s_symbols DEPENDS ${TZ_S_SYMBOLS_HEADER})
add_dependencies(tz_ns tz_s_symbols)
