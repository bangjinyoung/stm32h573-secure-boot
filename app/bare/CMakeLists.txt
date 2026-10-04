add_executable(bare main.c)
target_link_libraries(bare PRIVATE board)
target_compile_definitions(bare PRIVATE BUILD_ID="${BUILD_ID}")
target_link_options(bare PRIVATE
  -T${DEVICE_DIR}/Source/Templates/gcc/linker/STM32H573xx_FLASH.ld
  -Wl,-Map=$<TARGET_FILE_DIR:bare>/bare.map)
set_target_properties(bare PROPERTIES SUFFIX .elf)

add_custom_command(TARGET bare POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bare> $<TARGET_FILE_DIR:bare>/bare.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:bare>)
