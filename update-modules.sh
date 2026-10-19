#!/usr/bin/env bash
# Update vendor modules using the manifest published by l-evk-t2-gh3220.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
NCS_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

if [[ -x "${NCS_ROOT}/.venv/bin/python" ]]; then
	PYTHON="${NCS_ROOT}/.venv/bin/python"
elif command -v python3 >/dev/null 2>&1; then
	PYTHON="$(command -v python3)"
else
	echo "Error: Python 3 was not found." >&2
	exit 127
fi

exec "${PYTHON}" "${SCRIPT_DIR}/scripts/update_lierda_modules.py"
