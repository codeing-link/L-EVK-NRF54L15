# Match the nRF54L15 DK's CPU application-core J-Link configuration. The
# package name (QFAA) is not a valid J-Link target; J-Link selects the M33
# application core by this device name instead.
board_runner_args(jlink "--device=nRF54L15_M33" "--speed=4000")

# Prefer nRF Util for ordinary flash operations, matching the known-working
# nRF54L15 DK flow. flash_jlink.sh explicitly selects the J-Link runner.
include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
