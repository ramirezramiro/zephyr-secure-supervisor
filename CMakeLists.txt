cmake_minimum_required(VERSION 3.20.0)

if (BOARD STREQUAL "nucleo_l053r8")
  set(DTC_OVERLAY_FILE
      ${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo_l053r8_secure_supervisor.overlay
      ${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo_l053r8_secure_supervisor_app.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_secure_supervisor)

# Add app sources explicitly
target_sources(app PRIVATE
  src/simple_aes.c
  src/app_crypto.c
  src/main.c
  src/sensor_hts221.c
  src/supervisor.c
  src/recovery.c
  src/persist_state.c
  src/uart_commands.c
  src/watchdog_ctrl.c
)

# Allow #include "supervisor.h" etc without full path (autocomplete)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
