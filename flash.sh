#!/usr/bin/env bash
# Flash L-EVK-NRF54L15 with the selected Zephyr runner (normally nrfutil).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NCS_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
WEST_FLASH_ARGS=()

# Do not silently fall back to build/. This option must match the directory
# passed to build.sh when building a versioned, OTA, or diagnostic image.
while (($# > 0)); do
	case "$1" in
	--build-dir)
		if (($# < 2)); then
			echo "Error: --build-dir requires a path." >&2
			exit 2
		fi
		BUILD_DIR="$2"
		shift 2
		;;
	--build-dir=*)
		BUILD_DIR="${1#*=}"
		shift
		;;
	*)
		WEST_FLASH_ARGS+=("$1")
		shift
		;;
	esac
done

APP_BUILD_DIR="${BUILD_DIR}/L-EVK-NRF54L15"

if [[ -x "${NCS_ROOT}/.venv/bin/nrfutil" ]]; then
	export PATH="${NCS_ROOT}/.venv/bin:${PATH}"
	export NRFUTIL_HOME="${NRFUTIL_HOME:-${NCS_ROOT}/.venv/nrfutil}"
fi

if [[ ! -f "${APP_BUILD_DIR}/zephyr/zephyr.signed.hex" || \
	! -f "${BUILD_DIR}/zephyr/.config" ]] || \
	! grep -q '^SB_CONFIG_BOOTLOADER_MCUBOOT=y$' "${BUILD_DIR}/zephyr/.config"; then
	echo "Selected build directory is incomplete; rebuilding it: ${BUILD_DIR}" >&2
	BUILD_DIR="${BUILD_DIR}" "${SCRIPT_DIR}/build.sh"
fi

if [[ -x "${NCS_ROOT}/.venv/bin/west" ]]; then
	WEST=("${NCS_ROOT}/.venv/bin/west")
elif command -v west >/dev/null 2>&1; then
	WEST=("$(command -v west)")
else
	echo "Error: west was not found. Activate the NCS virtual environment first." >&2
	exit 127
fi

echo "Flashing sysbuild output from: ${BUILD_DIR}"
echo "Application image: ${APP_BUILD_DIR}/zephyr/zephyr.signed.hex"
exec "${WEST[@]}" flash --build-dir "${BUILD_DIR}" "${WEST_FLASH_ARGS[@]}"
