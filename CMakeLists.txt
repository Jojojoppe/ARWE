cmake_minimum_required(VERSION 3.16)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/extern/cubemx.cmake/arm-gcc.cmake)
include(extern/cubemx.cmake/cubemx.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(ARWE C)

add_executable(ARWE
  src/main.c
)
cubemx_target(TARGET ARWE 
  IOC ${CMAKE_CURRENT_SOURCE_DIR}/cube/ARWE.ioc
  CUBEMX_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cube
)
target_compile_options(ARWE PUBLIC -g)