#!/usr/bin/env bash
# Flash the built image through DAPLink using PyOCD.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
NCS_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
IMAGE="${1:-${SCRIPT_DIR}/build/L-EVK-NRF54L15/zephyr/zephyr.bin}"
TARGET="${PYOCD_TARGET:-nrf54l}"
FREQUENCY="${PYOCD_FREQUENCY:-1000000}"

if [[ ! "${FREQUENCY}" =~ ^[0-9]+$ ]]; then
	echo "Error: PYOCD_FREQUENCY must be an integer in Hz." >&2
	exit 2
fi

if [[ -x "${NCS_ROOT}/.venv/bin/pyocd" ]]; then
	PYOCD=("${NCS_ROOT}/.venv/bin/pyocd")
elif command -v pyocd >/dev/null 2>&1; then
	PYOCD=("$(command -v pyocd)")
else
	echo "Error: pyocd was not found in ${NCS_ROOT}/.venv/bin or PATH." >&2
	exit 127
fi

if [[ ! -f "${IMAGE}" ]]; then
	echo "Error: firmware image not found: ${IMAGE}" >&2
	exit 1
fi

echo "Mass-erasing ${TARGET} through DAPLink..."
"${PYOCD[@]}" erase -t "${TARGET}" -f "${FREQUENCY}" --mass

echo "Flashing ${IMAGE} at ${FREQUENCY} Hz..."
"${PYOCD[@]}" flash -t "${TARGET}" "${IMAGE}" -f "${FREQUENCY}"

echo "Resetting ${TARGET}..."
"${PYOCD[@]}" reset -t "${TARGET}"
