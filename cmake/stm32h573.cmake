set(STM32H573_BASE_LINKER_SCRIPT ${DEVICE_DIR}/Source/Templates/gcc/linker/STM32H573xx_FLASH.ld)

# Writes a copy of the CMSIS linker script with only the FLASH and RAM regions and the stack size changed, so every image shares one
# section layout.
function(stm32h573_linker_script out_file flash_origin flash_length ram_origin ram_length stack_size)
    file(READ ${STM32H573_BASE_LINKER_SCRIPT} script)
    string(REGEX REPLACE "FLASH +\\(rx\\) +: ORIGIN = 0x8000000, +LENGTH = 2048K" "FLASH (rx) : ORIGIN = ${flash_origin}, LENGTH = ${flash_length}" patched "${script}")
    string(REGEX REPLACE "RAM +\\(xrw\\) +: ORIGIN = 0x20000000, +LENGTH = 640K" "RAM (xrw) : ORIGIN = ${ram_origin}, LENGTH = ${ram_length}" patched "${patched}")
    string(REGEX REPLACE "_Min_Stack_Size = 0x400;" "_Min_Stack_Size = ${stack_size};" patched "${patched}")
    if(NOT patched MATCHES "ORIGIN = ${flash_origin}, LENGTH = ${flash_length}" OR NOT patched MATCHES "ORIGIN = ${ram_origin}, LENGTH = ${ram_length}"
       OR NOT patched MATCHES "_Min_Stack_Size = ${stack_size};")
        message(FATAL_ERROR "FLASH region, RAM region or stack size not found in ${STM32H573_BASE_LINKER_SCRIPT}")
    endif()
    file(WRITE ${out_file} "${patched}")
endfunction()

# Adds a Non-secure callable region after FLASH and places the CMSE veneers (.gnu.sgstubs) in it. __nsc_region_start
# and __nsc_region_end bound the whole region for the SAU.
function(stm32h573_linker_script_add_nsc script_file nsc_origin nsc_length)
    file(READ ${script_file} script)
    string(REGEX REPLACE "(FLASH \\(rx\\) : ORIGIN = [^\n]*\n)" "\\1  FLASH_NSC (rx) : ORIGIN = ${nsc_origin}, LENGTH = ${nsc_length}\n" patched "${script}")
    string(REPLACE "  /* Constant data into \"FLASH\" Rom type memory */" "  .gnu.sgstubs :
  {
    . = ALIGN(32);
    *(.gnu.sgstubs*)
    . = ALIGN(32);
  } >FLASH_NSC
  __nsc_region_start = ORIGIN(FLASH_NSC);
  __nsc_region_end = ORIGIN(FLASH_NSC) + LENGTH(FLASH_NSC);

  /* Constant data into \"FLASH\" Rom type memory */" patched "${patched}")
    if(NOT patched MATCHES "FLASH_NSC \\(rx\\) : ORIGIN = ${nsc_origin}" OR NOT patched MATCHES "\\.gnu\\.sgstubs")
        message(FATAL_ERROR "FLASH region or .rodata anchor not found in ${script_file}")
    endif()
    file(WRITE ${script_file} "${patched}")
endfunction()

function(stm32h573_check_range target what address start end)
    math(EXPR value "${address}")
    math(EXPR lower "${start}")
    math(EXPR upper "${end}")
    if(value LESS lower OR value GREATER_EQUAL upper)
        message(FATAL_ERROR "${target}: ${what} ${address} is outside ${start}-${end}")
    endif()
endfunction()

# stm32h573_executable(<target> FLASH <origin> <length> [RAM <origin> <length>] [STACK <size>] [NSC <origin> <length>] [NONSECURE])
#   Secure (default): flash and SRAM at the Secure aliases; RAM defaults to all of SRAM1-3 (0x30000000, 640K).
#   NONSECURE: flash and SRAM at the Non-secure aliases, built without -mcmse, RAM required. The Secure image sets
#   VTOR_NS, so the Non-secure SystemInit leaves VTOR alone.
#   NSC (Secure only): a Non-secure callable region for cmse_nonsecure_entry veneers, and <target>_cmse_implib.o, the
#   import library a Non-secure image links against.
#   STACK defaults to 0x400 from the CMSIS template.
function(stm32h573_executable target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "NONSECURE" "STACK" "FLASH;RAM;NSC")
    list(LENGTH ARG_FLASH flash_args)
    if(NOT flash_args EQUAL 2 OR ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "${target}: usage stm32h573_executable(<target> FLASH <origin> <length> [RAM <origin> <length>] [STACK <size>] [NSC <origin> <length>] [NONSECURE])")
    endif()
    if(ARG_NSC AND ARG_NONSECURE)
        message(FATAL_ERROR "${target}: only a Secure image has a Non-secure callable region")
    endif()
    list(GET ARG_FLASH 0 flash_origin)
    list(GET ARG_FLASH 1 flash_length)
    if(NOT ARG_STACK)
        set(ARG_STACK 0x400)
    endif()
    if(NOT ARG_RAM)
        if(ARG_NONSECURE)
            message(FATAL_ERROR "${target}: a Non-secure image needs RAM <origin> <length>")
        endif()
        set(ARG_RAM 0x30000000 640K)
    endif()
    list(GET ARG_RAM 0 ram_origin)
    list(GET ARG_RAM 1 ram_length)

    if(ARG_NONSECURE)
        stm32h573_check_range(${target} flash ${flash_origin} 0x08000000 0x08200000)
        stm32h573_check_range(${target} RAM ${ram_origin} 0x20000000 0x200A0000)
        set_target_properties(${target} PROPERTIES STM32H573_NONSECURE ON)
        target_sources(${target} PRIVATE
            ${DEVICE_DIR}/Source/Templates/system_stm32h5xx_ns.c
            ${DEVICE_DIR}/Source/Templates/gcc/startup_stm32h573xx.s)
    else()
        stm32h573_check_range(${target} flash ${flash_origin} 0x0C000000 0x0C200000)
        stm32h573_check_range(${target} RAM ${ram_origin} 0x30000000 0x300A0000)
        # SystemInit writes VTOR unconditionally, so each image builds its own copy with its own vector table offset.
        math(EXPR vector_offset "${flash_origin} - 0x0C000000" OUTPUT_FORMAT HEXADECIMAL)
        target_sources(${target} PRIVATE
            ${DEVICE_DIR}/Source/Templates/system_stm32h5xx.c
            ${DEVICE_DIR}/Source/Templates/gcc/startup_stm32h573xx.s)
        set_source_files_properties(${DEVICE_DIR}/Source/Templates/system_stm32h5xx.c
            TARGET_DIRECTORY ${target} PROPERTIES COMPILE_DEFINITIONS VECT_TAB_OFFSET=${vector_offset}U)
    endif()

    set(script ${CMAKE_CURRENT_BINARY_DIR}/${target}.ld)
    stm32h573_linker_script(${script} ${flash_origin} ${flash_length} ${ram_origin} ${ram_length} ${ARG_STACK})
    if(ARG_NSC)
        list(GET ARG_NSC 0 nsc_origin)
        list(GET ARG_NSC 1 nsc_length)
        stm32h573_check_range(${target} NSC ${nsc_origin} 0x0C000000 0x0C200000)
        stm32h573_linker_script_add_nsc(${script} ${nsc_origin} ${nsc_length})
        set(implib ${CMAKE_CURRENT_BINARY_DIR}/${target}_cmse_implib.o)
        target_link_options(${target} PRIVATE -Wl,--cmse-implib -Wl,--out-implib=${implib})
        set_target_properties(${target} PROPERTIES STM32H573_CMSE_IMPLIB ${implib})
        # The linker writes the import library; declare it so a Non-secure image linking it waits for this link.
        add_custom_command(TARGET ${target} POST_BUILD BYPRODUCTS ${implib} COMMAND ${CMAKE_COMMAND} -E true)
    endif()

    add_dependencies(${target} build_id)
    target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/generated)
    target_link_options(${target} PRIVATE -T${script} -Wl,-Map=$<TARGET_FILE_DIR:${target}>/${target}.map)
    set_target_properties(${target} PROPERTIES SUFFIX .elf LINK_DEPENDS ${script})

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> $<TARGET_FILE_DIR:${target}>/${target}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${target}>)
endfunction()
