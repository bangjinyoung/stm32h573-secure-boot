# Non-secure app (MCUboot image 1) in the image 1 primary slot after its header; 62K leaves room for the TLVs in the
# 0x10000 slot. SRAM3, which the Secure app makes Non-secure.
add_executable(tz_ns main.c)
target_link_libraries(tz_ns PRIVATE stm32h573_device)
stm32h573_executable(tz_ns FLASH 0x08020400 62K RAM 0x20050000 320K NONSECURE)
