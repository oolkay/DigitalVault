#!/usr/bin/env bash
# HAL serial test via socat PTY pair + simulator --uart
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TTY_DEV="/tmp/biometric_vault_tty"
TTY_HOST="/tmp/biometric_vault_tty_host"

if ! command -v socat >/dev/null; then
  echo "socat required: sudo apt install socat" >&2
  exit 1
fi

pkill -f "socat.*biometric_vault_tty" 2>/dev/null || true
pkill -f "hw_simulator/simulator" 2>/dev/null || true
sleep 0.3

socat "pty,link=$TTY_DEV,raw,echo=0" "pty,link=$TTY_HOST,raw,echo=0" &
SOCAT_PID=$!
sleep 0.5

cd "$ROOT/hw_simulator"
BIOMETRIC_UART="$TTY_DEV" timeout 45 python3 simulator.py -v &
SIM_PID=$!
sleep 2

if [[ ! -e "$TTY_HOST" ]]; then
  echo "PTY host missing: $TTY_HOST" >&2
  kill "$SIM_PID" "$SOCAT_PID" 2>/dev/null || true
  exit 1
fi

timeout 30 "$ROOT/build/bin/test_hal" "serial:$TTY_HOST"
RET=$?

kill "$SIM_PID" "$SOCAT_PID" 2>/dev/null || true
wait "$SIM_PID" 2>/dev/null || true
exit $RET
