cmake_minimum_required(VERSION 3.22)

project(stm32h573_secure_boot C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g")

set(MODULES_DIR ${CMAKE_SOURCE_DIR}/modules)
set(HAL_DIR ${MODULES_DIR}/stm32h5xx_hal_driver)
set(DEVICE_DIR ${MODULES_DIR}/cmsis_device_h5)
set(PLATFORM_DIR ${CMAKE_SOURCE_DIR}/platform/stm32h573)

include(cmake/stm32h573.cmake)

# A build directory configured before -mcmse became per-target keeps it in the cached CMAKE_C_FLAGS, which would
# build the Non-secure app as Secure code without any error.
if(CMAKE_C_FLAGS MATCHES "-mcmse")
  message(FATAL_ERROR "CMAKE_C_FLAGS still contains -mcmse from an old cache; delete ${CMAKE_BINARY_DIR} and configure again")
endif()

# TZEN=1: every target is Secure unless it sets STM32H573_NONSECURE. -mcmse makes CMSIS pick the _S memory and
# peripheral aliases; a Non-secure target keeps the plain aliases.
add_compile_options("$<$<AND:$<COMPILE_LANGUAGE:C>,$<NOT:$<BOOL:$<TARGET_PROPERTY:STM32H573_NONSECURE>>>>:-mcmse>")

set(BUILD_ID_HEADER ${CMAKE_BINARY_DIR}/generated/build_id.h)
add_custom_target(build_id
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DOUTPUT=${BUILD_ID_HEADER} -P ${CMAKE_SOURCE_DIR}/cmake/build_id.cmake
  BYPRODUCTS ${BUILD_ID_HEADER})

add_library(stm32h5_hal STATIC
  ${HAL_DIR}/Src/stm32h5xx_hal.c
  ${HAL_DIR}/Src/stm32h5xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_dma.c
  ${HAL_DIR}/Src/stm32h5xx_hal_dma_ex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_flash.c
  ${HAL_DIR}/Src/stm32h5xx_hal_flash_ex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32h5xx_hal_gtzc.c
  ${HAL_DIR}/Src/stm32h5xx_hal_pwr.c
  ${HAL_DIR}/Src/stm32h5xx_hal_pwr_ex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32h5xx_hal_rcc_ex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_uart.c
  ${HAL_DIR}/Src/stm32h5xx_hal_uart_ex.c)

# CMSIS device headers only, for images that do not link the Secure HAL (the Non-secure app).
add_library(stm32h573_device INTERFACE)
target_include_directories(stm32h573_device INTERFACE
  ${DEVICE_DIR}/Include
  ${MODULES_DIR}/cmsis_core/Core/Include)
target_compile_definitions(stm32h573_device INTERFACE STM32H573xx)

target_include_directories(stm32h5_hal PUBLIC
  ${PLATFORM_DIR}
  ${HAL_DIR}/Inc)
target_compile_definitions(stm32h5_hal PUBLIC USE_HAL_DRIVER)
target_link_libraries(stm32h5_hal PUBLIC stm32h573_device)

add_subdirectory(boards/stm32h573i_dk)
add_subdirectory(app/bare)
add_subdirectory(app/tz_s)
add_subdirectory(app/tz_ns)
add_subdirectory(boot/mcuboot)
