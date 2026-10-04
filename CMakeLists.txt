cmake_minimum_required(VERSION 3.22)

project(stm32h573_secure_boot C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g")

set(MODULES_DIR ${CMAKE_SOURCE_DIR}/modules)
set(HAL_DIR ${MODULES_DIR}/stm32h5xx_hal_driver)
set(DEVICE_DIR ${MODULES_DIR}/cmsis_device_h5)
set(PLATFORM_DIR ${CMAKE_SOURCE_DIR}/platform/stm32h573)

execute_process(
  COMMAND git describe --always --dirty
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE BUILD_ID
  OUTPUT_STRIP_TRAILING_WHITESPACE)

add_library(stm32h5_hal STATIC
  ${HAL_DIR}/Src/stm32h5xx_hal.c
  ${HAL_DIR}/Src/stm32h5xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_dma.c
  ${HAL_DIR}/Src/stm32h5xx_hal_dma_ex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32h5xx_hal_pwr.c
  ${HAL_DIR}/Src/stm32h5xx_hal_pwr_ex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32h5xx_hal_rcc_ex.c
  ${HAL_DIR}/Src/stm32h5xx_hal_uart.c
  ${HAL_DIR}/Src/stm32h5xx_hal_uart_ex.c
  ${DEVICE_DIR}/Source/Templates/system_stm32h5xx.c
  ${DEVICE_DIR}/Source/Templates/gcc/startup_stm32h573xx.s)

target_include_directories(stm32h5_hal PUBLIC
  ${PLATFORM_DIR}
  ${HAL_DIR}/Inc
  ${DEVICE_DIR}/Include
  ${MODULES_DIR}/cmsis_core/Core/Include)

target_compile_definitions(stm32h5_hal PUBLIC STM32H573xx USE_HAL_DRIVER)

add_subdirectory(boards/stm32h573i_dk)
add_subdirectory(app/bare)
