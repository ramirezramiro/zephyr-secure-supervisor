cmake_minimum_required(VERSION 3.20.0)
set(APP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(KCONFIG_ROOT ${APP_ROOT}/Kconfig)
set(BOARD_ROOT ${APP_ROOT})

set(DTC_OVERLAY_FILE ${APP_ROOT}/tests/common/native.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(persist_state_tests)

target_sources(app PRIVATE
  ${APP_ROOT}/src/app_crypto.c
  ${APP_ROOT}/src/curve25519_ref10.c
  ${APP_ROOT}/src/simple_aes.c
  ${APP_ROOT}/src/persist_state.c
  src/main.c
)

target_include_directories(app PRIVATE ${APP_ROOT}/src)
