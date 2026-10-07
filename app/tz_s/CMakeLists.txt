# Secure app (MCUboot image 0) in the image 0 primary slot after its header. SRAM1 only: the app hands SRAM3 to the
# Non-secure app, and SRAM2 stays unused. The veneers sit right after the code so the image signature covers them;
# the 2K left at the end of the 0x8000 slot holds the TLVs.
add_executable(tz_s main.c tz_partition.c nsc.c)
target_link_libraries(tz_s PRIVATE board)
# Slot addresses come from the bootloader layout, the one place that defines them.
target_include_directories(tz_s PRIVATE ${CMAKE_SOURCE_DIR}/boot/mcuboot/config PUBLIC include)
stm32h573_executable(tz_s FLASH 0x0C018400 28K RAM 0x30000000 256K NSC 0x0C01F400 1K)
