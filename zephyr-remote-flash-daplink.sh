#!/usr/bin/env bash
# Flash through the DAPLink probe attached to the Ubuntu host.

set -euo pipefail

UBUNTU_HOST="${UBUNTU_HOST:-qinbo@192.168.1.100}"
REMOTE_PROJECT="${REMOTE_PROJECT:-/home/qinbo/mpushare/macos_workspace/nordic/ncs/apps/L-EVK-NRF54L15}"
PYOCD_FREQUENCY="${PYOCD_FREQUENCY:-1000000}"

if [[ ! "${PYOCD_FREQUENCY}" =~ ^[0-9]+$ ]]; then
	echo "Error: PYOCD_FREQUENCY must be an integer in Hz." >&2
	exit 2
fi

ssh -o ServerAliveInterval=30 "${UBUNTU_HOST}" \
	"cd '${REMOTE_PROJECT}' && PYOCD_FREQUENCY='${PYOCD_FREQUENCY}' bash ./flash_daplink.sh $*"
