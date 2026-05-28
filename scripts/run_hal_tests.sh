#!/usr/bin/env bash
# Run HAL tests against JSON simulator socket.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SIM_PID=""
cleanup() {
  if [[ -n "$SIM_PID" ]] && kill -0 "$SIM_PID" 2>/dev/null; then
    kill "$SIM_PID" 2>/dev/null || true
    wait "$SIM_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

cd "$ROOT/hw_simulator"
python3 simulator.py -v &
SIM_PID=$!
sleep 1.5

if [[ ! -S /tmp/biometric_vault.sock ]]; then
  echo "Simulator socket missing" >&2
  exit 1
fi

"$ROOT/build/bin/test_hal" "simulator:/tmp/biometric_vault.sock"
