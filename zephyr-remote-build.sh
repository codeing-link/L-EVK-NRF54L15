#!/usr/bin/env bash
# Build on the Ubuntu NCS host. Override UBUNTU_HOST or REMOTE_PROJECT.

set -euo pipefail

UBUNTU_HOST="${UBUNTU_HOST:-qinbo@192.168.1.100}"
REMOTE_PROJECT="${REMOTE_PROJECT:-/home/qinbo/mpushare/macos_workspace/nordic/ncs/apps/L-EVK-NRF54L15}"

# SSH executes a command through a remote shell. Quote every argument so a
# Kconfig string value such as -DCONFIG_...=\"1.0.1+0\" reaches CMake intact.
printf -v REMOTE_COMMAND 'cd %q && bash ./build.sh' "${REMOTE_PROJECT}"
for argument in "$@"; do
	printf -v QUOTED_ARGUMENT ' %q' "${argument}"
	REMOTE_COMMAND+="${QUOTED_ARGUMENT}"
done

exec ssh -o ServerAliveInterval=30 "${UBUNTU_HOST}" "${REMOTE_COMMAND}"
