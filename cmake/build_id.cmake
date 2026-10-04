# Run at build time so the ID matches the sources that were actually compiled.
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE build_id
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(content "#define BUILD_ID \"${build_id}\"\n")
if(EXISTS ${OUTPUT})
    file(READ ${OUTPUT} current)
endif()
if(NOT current STREQUAL content)
    file(WRITE ${OUTPUT} "${content}")
endif()
