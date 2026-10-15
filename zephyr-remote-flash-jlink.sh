#!/usr/bin/env bash
# Flash through a J-Link probe connected to the remote NCS host.

set -euo pipefail

UBUNTU_HOST="${UBUNTU_HOST:-qinbo@192.168.1.100}"
REMOTE_PROJECT="${REMOTE_PROJECT:-/home/qinbo/mpushare/macos_workspace/nordic/ncs/apps/L-EVK-NRF54L15}"
ssh -o ServerAliveInterval=30 "${UBUNTU_HOST}" \
	"cd '${REMOTE_PROJECT}' && bash ./flash_jlink.sh $*"
