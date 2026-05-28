#!/usr/bin/env bash
# MVP Definition of Done — simulator checks (socket + UART).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0

echo "=== DoD 1: HAL socket suite ==="
"$ROOT/scripts/run_hal_tests.sh" || FAIL=1

echo ""
echo "=== DoD 2: 10× verify + get_key (socket) ==="
cd "$ROOT/hw_simulator"
python3 simulator.py -v &
SIM_PID=$!
sleep 1.5
if ! "$ROOT/build/bin/test_hal" "simulator:/tmp/biometric_vault.sock" --stress 10; then
  FAIL=1
fi
kill "$SIM_PID" 2>/dev/null || true
wait "$SIM_PID" 2>/dev/null || true

echo ""
echo "=== DoD 3: UART binary (socat PTY) ==="
if command -v socat >/dev/null; then
  "$ROOT/scripts/simulator_uart_test.sh" || FAIL=1
else
  echo "SKIP: socat not installed"
fi

echo ""
if [[ $FAIL -eq 0 ]]; then
  echo "DoD: ALL CHECKS PASSED"
else
  echo "DoD: SOME CHECKS FAILED"
  exit 1
fi
