#!/usr/bin/env bash
# Flash a board connected to the machine on which this script is run.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NCS_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
FLASH_BUILD_DIR="${FLASH_BUILD_DIR:-${BUILD_DIR}/L-EVK-NRF54L15}"

if [[ -x "${NCS_ROOT}/.venv/bin/west" ]]; then
	WEST=("${NCS_ROOT}/.venv/bin/west")
elif command -v west >/dev/null 2>&1; then
	WEST=("$(command -v west)")
else
	echo "Error: west was not found in ${NCS_ROOT}/.venv/bin or PATH." >&2
	exit 127
fi

# NCS uses sysbuild, so the application image and runner configuration are
# stored beneath the application name instead of directly in BUILD_DIR.
if [[ ! -f "${FLASH_BUILD_DIR}/zephyr/zephyr.hex" ]]; then
	"${SCRIPT_DIR}/build.sh"
fi

exec "${WEST[@]}" flash --build-dir "${FLASH_BUILD_DIR}" --runner jlink "$@"
