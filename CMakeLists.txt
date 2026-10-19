cmake_minimum_required(VERSION 3.20.0)

# Keep the custom board versioned together with the application repository.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(l_evk_nrf54l15)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/imu_lis2dw12.c src/ppg_gh3220_hal.c)
target_sources_ifdef(CONFIG_MCUMGR_TRANSPORT_BT app PRIVATE src/smp_bt.c)
