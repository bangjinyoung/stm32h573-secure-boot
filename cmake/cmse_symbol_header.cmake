# Writes the address of one Secure symbol as a C macro, read from the Secure image with nm at build time.
# usage: cmake -DNM=<nm> -DELF=<secure elf> -DSYMBOL=<name> -DMACRO=<macro> -DOUTPUT=<header> -P cmse_symbol_header.cmake
execute_process(
    COMMAND ${NM} ${ELF}
    OUTPUT_VARIABLE symbols
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${NM} ${ELF} failed: ${rc}")
endif()
string(REGEX MATCH "([0-9a-f]+) [Tt] ${SYMBOL}\n" match "${symbols}")
if(NOT match)
    message(FATAL_ERROR "${SYMBOL} not found in ${ELF}")
endif()
set(content "#define ${MACRO} 0x${CMAKE_MATCH_1}U\n")
if(EXISTS ${OUTPUT})
    file(READ ${OUTPUT} current)
endif()
if(NOT current STREQUAL content)
    file(WRITE ${OUTPUT} "${content}")
endif()
