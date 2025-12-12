cmake_minimum_required(VERSION 3.20.0)

set(APP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CONF_FILE ${APP_ROOT}/prj.conf)
set(EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf)

if (BOARD STREQUAL "nucleo_l053r8")
  set(DTC_OVERLAY_FILE
      ${APP_ROOT}/boards/nucleo_l053r8_secure_supervisor.overlay
      ${APP_ROOT}/boards/nucleo_l053r8_secure_supervisor_app.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(thread_fail_wrapper)

set(APP_COMMON_SRCS
  ${APP_ROOT}/src/simple_aes.c
  ${APP_ROOT}/src/app_crypto.c
  ${APP_ROOT}/src/main.c
  ${APP_ROOT}/src/supervisor.c
  ${APP_ROOT}/src/recovery.c
  ${APP_ROOT}/src/persist_state.c
  ${APP_ROOT}/src/uart_commands.c
  ${APP_ROOT}/src/watchdog_ctrl.c
)

if (CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
  list(APPEND APP_COMMON_SRCS ${APP_ROOT}/src/curve25519_ref10.c)
endif()

target_sources(app PRIVATE ${APP_COMMON_SRCS})
target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sensor_stub.c
)

target_include_directories(app PRIVATE ${APP_ROOT}/src)
