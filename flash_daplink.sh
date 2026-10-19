#!/usr/bin/env bash
# Flash a complete MCUboot + application installation through DAPLink/PyOCD.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
NCS_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BOOTLOADER_IMAGE="${BUILD_DIR}/mcuboot/zephyr/zephyr.hex"
APPLICATION_IMAGE="${BUILD_DIR}/L-EVK-NRF54L15/zephyr/zephyr.signed.hex"
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

if [[ $# -gt 0 ]]; then
	echo "Error: flash_daplink.sh always flashes the canonical build/ output; no image path is accepted." >&2
	exit 2
fi

IMAGES=("${BOOTLOADER_IMAGE}" "${APPLICATION_IMAGE}")

for image in "${IMAGES[@]}"; do
	if [[ ! -f "${image}" ]]; then
		echo "Canonical build/ output is incomplete; rebuilding it first." >&2
		"${SCRIPT_DIR}/build.sh"
		break
	fi
done

for image in "${IMAGES[@]}"; do
	if [[ ! -f "${image}" ]]; then
		echo "Error: firmware image not found after build: ${image}" >&2
		exit 1
	fi
done

echo "Flashing canonical build directory: ${BUILD_DIR}"
echo "MCUboot image: ${BOOTLOADER_IMAGE}"
echo "Application image: ${APPLICATION_IMAGE}"
"${PYOCD[@]}" erase -t "${TARGET}" -f "${FREQUENCY}" --mass
for image in "${IMAGES[@]}"; do
	"${PYOCD[@]}" flash -t "${TARGET}" "${image}" -f "${FREQUENCY}"
done
"${PYOCD[@]}" reset -t "${TARGET}"
