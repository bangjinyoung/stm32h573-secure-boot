add_library(board STATIC board.c)
target_include_directories(board PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(board PUBLIC stm32h5_hal)
