set(STM32H573_BASE_LINKER_SCRIPT ${DEVICE_DIR}/Source/Templates/gcc/linker/STM32H573xx_FLASH.ld)

# Writes a copy of the CMSIS linker script with only the FLASH region and stack size changed, so every image shares one section layout.
function(stm32h573_linker_script out_file flash_origin flash_length stack_size)
    file(READ ${STM32H573_BASE_LINKER_SCRIPT} script)
    string(REGEX REPLACE "FLASH +\\(rx\\) +: ORIGIN = 0x8000000, +LENGTH = 2048K" "FLASH (rx) : ORIGIN = ${flash_origin}, LENGTH = ${flash_length}" patched "${script}")
    string(REGEX REPLACE "_Min_Stack_Size = 0x400;" "_Min_Stack_Size = ${stack_size};" patched "${patched}")
    if(NOT patched MATCHES "ORIGIN = ${flash_origin}" OR NOT patched MATCHES "_Min_Stack_Size = ${stack_size};")
        message(FATAL_ERROR "FLASH region or stack size not found in ${STM32H573_BASE_LINKER_SCRIPT}")
    endif()
    file(WRITE ${out_file} "${patched}")
endfunction()

# Optional fourth argument: stack size, default 0x400 from the CMSIS template.
function(stm32h573_executable target flash_origin flash_length)
    set(stack_size 0x400)
    if(ARGC GREATER 3)
        set(stack_size ${ARGV3})
    endif()

    set(script ${CMAKE_CURRENT_BINARY_DIR}/${target}.ld)
    stm32h573_linker_script(${script} ${flash_origin} ${flash_length} ${stack_size})

    # SystemInit writes VTOR unconditionally, so each image builds its own copy with its own vector table offset.
    math(EXPR vector_offset "${flash_origin} - 0x08000000" OUTPUT_FORMAT HEXADECIMAL)
    target_sources(${target} PRIVATE
        ${DEVICE_DIR}/Source/Templates/system_stm32h5xx.c
        ${DEVICE_DIR}/Source/Templates/gcc/startup_stm32h573xx.s)
    set_source_files_properties(${DEVICE_DIR}/Source/Templates/system_stm32h5xx.c
        TARGET_DIRECTORY ${target} PROPERTIES COMPILE_DEFINITIONS VECT_TAB_OFFSET=${vector_offset}U)

    target_compile_definitions(${target} PRIVATE BUILD_ID="${BUILD_ID}")
    target_link_options(${target} PRIVATE -T${script} -Wl,-Map=$<TARGET_FILE_DIR:${target}>/${target}.map)
    set_target_properties(${target} PROPERTIES SUFFIX .elf LINK_DEPENDS ${script})

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> $<TARGET_FILE_DIR:${target}>/${target}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${target}>)
endfunction()
