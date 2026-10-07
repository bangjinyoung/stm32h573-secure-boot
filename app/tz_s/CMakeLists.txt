# Secure app (MCUboot image 0) in the image 0 primary slot after its header. SRAM1 only: the app hands SRAM3 to the
# Non-secure app, and SRAM2 stays unused.
add_executable(tz_s main.c)
target_link_libraries(tz_s PRIVATE board)
stm32h573_executable(tz_s FLASH 0x0C018400 30K RAM 0x30000000 256K)
