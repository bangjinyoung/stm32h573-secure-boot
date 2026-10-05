# The application linked into the MCUboot primary slot, after the image header; 30K leaves room for the TLVs in the 0x8000 slot.
add_executable(bare_slot main.c)
target_link_libraries(bare_slot PRIVATE board)
stm32h573_executable(bare_slot 0x0C018400 30K)

set(APP_VERSION "1.0.0" CACHE STRING "Version written into the signed image header")
set(APP_SECURITY_COUNTER "1" CACHE STRING "Security counter TLV the HDPL2 rollback record compares against")
set(MCUBOOT_SIGNING_KEY ${CMAKE_SOURCE_DIR}/.workspace/keys/mcuboot-ecdsa-p256.pem CACHE FILEPATH "Private key for imgtool sign")

if(EXISTS ${MCUBOOT_SIGNING_KEY})
    add_custom_command(TARGET bare_slot POST_BUILD
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/imgtool sign
            --key ${MCUBOOT_SIGNING_KEY}
            --header-size 0x400 --pad-header
            --align 16
            --slot-size 0x8000
            --version ${APP_VERSION}
            --security-counter ${APP_SECURITY_COUNTER}
            $<TARGET_FILE_DIR:bare_slot>/bare_slot.bin
            $<TARGET_FILE_DIR:bare_slot>/bare_slot_signed.bin)
else()
    message(WARNING "MCUBOOT_SIGNING_KEY not found; bare_slot is built unsigned")
endif()
