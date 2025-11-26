cmake_minimum_required(VERSION 3.20.0)
set(APP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(KCONFIG_ROOT ${APP_ROOT}/Kconfig)
set(BOARD_ROOT ${APP_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(supervisor_tests)

target_sources(app PRIVATE
  ${APP_ROOT}/src/supervisor.c
  src/main.c
  src/stubs.c
)

target_include_directories(app PRIVATE ${APP_ROOT}/src)
