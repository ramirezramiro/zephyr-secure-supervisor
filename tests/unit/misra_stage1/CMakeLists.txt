cmake_minimum_required(VERSION 3.20.0)
set(APP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(KCONFIG_ROOT ${APP_ROOT}/Kconfig)
set(BOARD_ROOT ${APP_ROOT})
if(BOARD STREQUAL "nucleo_l053r8")
  set(DTC_OVERLAY_FILE
      ${APP_ROOT}/boards/nucleo_l053r8_secure_supervisor.overlay
      ${APP_ROOT}/boards/nucleo_l053r8_secure_supervisor_app.overlay)
endif()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(misra_stage1_tests)

target_sources(app PRIVATE
  ${APP_ROOT}/src/app_crypto.c
  ${APP_ROOT}/src/simple_aes.c
  ${APP_ROOT}/src/persist_state.c
  ${APP_ROOT}/src/recovery.c
  ${APP_ROOT}/src/supervisor.c
  src/main.c
  src/mock_watchdog_ctrl.c
)

target_include_directories(app PRIVATE ${APP_ROOT}/src)
