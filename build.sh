#!/usr/bin/env bash
# Build L-EVK-NRF54L15 from an NCS checkout. Run from any directory.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NCS_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BOARD="${BOARD:-l_evk_nrf54l15/nrf54l15/cpuapp}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"

if [[ -x "${NCS_ROOT}/.venv/bin/west" ]]; then
	WEST=("${NCS_ROOT}/.venv/bin/west")
elif command -v west >/dev/null 2>&1; then
	WEST=("$(command -v west)")
else
	echo "west was not found. Activate the NCS virtual environment first." >&2
	exit 1
fi

exec "${WEST[@]}" build --board "${BOARD}" --build-dir "${BUILD_DIR}" --pristine=auto "${SCRIPT_DIR}" \
	-- -DBOARD_ROOT="${SCRIPT_DIR}" "$@"
