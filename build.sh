#!/usr/bin/env bash
# Build the L-EVK application with its native Zephyr board target.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NCS_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BOARD="${BOARD:-l_evk_nrf54l15/nrf54l15/cpuapp}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
PRISTINE="${PRISTINE:-auto}"
WEST_BUILD_ARGS=()
LIERDA_MODULE_ROOT="${NCS_ROOT}/modules/lierda"
LIERDA_MODULES=(gh3220_sdk os_adapter core_framework)

for module in "${LIERDA_MODULES[@]}"; do
	module_path="${LIERDA_MODULE_ROOT}/${module}"
	if [[ ! -d "${module_path}" ]]; then
		echo "Error: missing ${module_path}; run ./update-modules.sh first." >&2
		exit 2
	fi
done

# Keep a custom build directory explicit and reusable by flash.sh. Remaining
# arguments are forwarded to CMake after the application-local BOARD_ROOT.
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
	--)
		shift
		WEST_BUILD_ARGS+=("$@")
		break
		;;
	*)
		WEST_BUILD_ARGS+=("$1")
		shift
		;;
	esac
done

# A previous non-sysbuild build cannot be upgraded in place to an MCUboot
# sysbuild. Detect that case so adding OTA support remains a one-command build.
if [[ "${PRISTINE}" == "auto" && -f "${BUILD_DIR}/zephyr/.config" ]]; then
	if ! grep -q '^SB_CONFIG_BOOTLOADER_MCUBOOT=y$' "${BUILD_DIR}/zephyr/.config" || \
		[[ ! -f "${BUILD_DIR}/mcuboot/zephyr/zephyr.dts" ]] || \
		! grep -q 'slot0_partition: partition@11000' "${BUILD_DIR}/mcuboot/zephyr/zephyr.dts" || \
		[[ ! -f "${BUILD_DIR}/L-EVK-NRF54L15/zephyr/zephyr.dts" ]] || \
		! grep -q 'l_evk_leds' "${BUILD_DIR}/L-EVK-NRF54L15/zephyr/zephyr.dts"; then
		PRISTINE="always"
	fi
fi

# A copied or renamed sysbuild directory can retain an APPLICATION_BINARY_DIR
# that points to the old path. CMake then tries to build that stale directory
# even though west was invoked with --build-dir "${BUILD_DIR}".
if [[ "${PRISTINE}" == "auto" && -f "${BUILD_DIR}/CMakeCache.txt" ]] && \
	! grep -Fqx "APPLICATION_BINARY_DIR:PATH=${BUILD_DIR}" "${BUILD_DIR}/CMakeCache.txt"; then
	# `west --pristine=always` does not reliably recover a sysbuild directory
	# whose nested CMake caches were copied from another build directory.  The
	# canonical default output is safe to recreate in full; custom directories
	# remain non-destructive and are handed to west's pristine mode instead.
	if [[ "${BUILD_DIR}" == "${SCRIPT_DIR}/build" ]]; then
		echo "Build cache points to a different directory; removing stale default build output." >&2
		rm -rf -- "${BUILD_DIR}"
	else
		echo "Build cache points to a different directory; running a pristine build." >&2
		PRISTINE="always"
	fi
fi

if [[ -x "${NCS_ROOT}/.venv/bin/west" ]]; then
	WEST=("${NCS_ROOT}/.venv/bin/west")
elif command -v west >/dev/null 2>&1; then
	WEST=("$(command -v west)")
else
	echo "Error: west was not found. Activate the NCS virtual environment first." >&2
	exit 127
fi

echo "Building sysbuild output in: ${BUILD_DIR}"

exec "${WEST[@]}" build --board "${BOARD}" --build-dir "${BUILD_DIR}" \
	--pristine="${PRISTINE}" "${SCRIPT_DIR}" -- \
	-DBOARD_ROOT="${SCRIPT_DIR}" "${WEST_BUILD_ARGS[@]}"
