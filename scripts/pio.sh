#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ -x /workspace/tools/halloween-venv/bin/pio ]]; then
  export PLATFORMIO_CORE_DIR=/workspace/tools/pioarduino
  exec /workspace/tools/halloween-venv/bin/pio "$@"
fi
exec pio "$@"
