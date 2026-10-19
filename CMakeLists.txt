cmake_minimum_required(VERSION 3.20.0)

# Keep the custom board versioned together with the application repository.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(LIERDA_MODULE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/lierda)
set(GH3220_APP_ROOT ${LIERDA_MODULE_ROOT}/l-evk-t2-gh3220/app_cardiff_a)

foreach(module gh3220_sdk os_adapter core_framework)
  if(NOT EXISTS ${LIERDA_MODULE_ROOT}/${module}/zephyr/module.yml AND
     NOT EXISTS ${LIERDA_MODULE_ROOT}/${module}/module.yml)
    message(FATAL_ERROR
      "Missing Lierda module '${module}'. Run ./update-modules.sh first.")
  endif()
  list(APPEND ZEPHYR_EXTRA_MODULES ${LIERDA_MODULE_ROOT}/${module})
endforeach()

if(NOT EXISTS ${GH3220_APP_ROOT}/src/gh3220_framework.c)
  message(FATAL_ERROR
    "Missing l-evk-t2-gh3220 example. Run ./update-modules.sh first.")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(l_evk_nrf54l15)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${GH3220_APP_ROOT}/src
)

target_sources(app PRIVATE
  src/main.c
  src/imu_lis2dw12.c
  src/gh3220_ble_service.c
  ${GH3220_APP_ROOT}/src/gh3220_sdk/gh3220_app_hook_zephyr.c
  ${GH3220_APP_ROOT}/src/gh3220_framework.c
  ${GH3220_APP_ROOT}/src/gh3220_protocol_user.c
  ${GH3220_APP_ROOT}/src/gh3220_frame.c
  ${GH3220_APP_ROOT}/src/gh3220_scheduler.c
)
