#!/usr/bin/env bash
# Reset DigitalVault local test state: runtime, registry, containers, sensor templates.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${DIGITALVAULT_BUILD:-$ROOT/build}"
VM="$BUILD/bin/vault_manager"
TEST_HAL="$BUILD/bin/test_hal"
MOUNT="${VAULT_MOUNT:-$HOME/VaultMount}"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}/DigitalVault"
SERIAL_PORT="${SERIAL_PORT:-/dev/ttyUSB0}"
HAL_PATH="serial:${SERIAL_PORT}"
CONF="${XDG_CONFIG_HOME:-$HOME/.config}/DigitalVault/VaultGuard.conf"

usage() {
  cat <<EOF
Usage: $0 [options]

  Full reset for a clean GUI/setup test:
    - vault reconcile/forceclose + mapper
    - detach loop devices backed by \$DATA/containers/*.img
    - remove vault_registry.json, vault_state.json, container images
    - clear QSettings sensor/occupiedSlots cache
    - wipe AS608 templates via HAL (--wipe-sensor)

Options:
  --skip-sensor     Do not wipe fingerprint sensor (files/loops only)
  --port PATH       Serial device (default: $SERIAL_PORT)
  --no-pkexec       Run vault_manager without pkexec (if already root)
  -h, --help        Show this help

Environment:
  DIGITALVAULT_BUILD  Build directory (default: $ROOT/build)
  VAULT_MOUNT         Mount point (default: ~/VaultMount)
  SERIAL_PORT         Short path for HAL (default: /dev/ttyUSB0)
EOF
}

SKIP_SENSOR=0
USE_PKEXEC=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-sensor) SKIP_SENSOR=1; shift ;;
    --port)
      SERIAL_PORT="$2"
      HAL_PATH="serial:${SERIAL_PORT}"
      shift 2
      ;;
    --no-pkexec) USE_PKEXEC=0; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
done

run_vm() {
  if [[ "$USE_PKEXEC" -eq 1 ]] && [[ "$(id -u)" -ne 0 ]]; then
    pkexec "$VM" "$@"
  else
    "$VM" "$@"
  fi
}

echo "==> [1/4] Vault runtime (mount: $MOUNT)"
if [[ -x "$VM" ]]; then
  run_vm reconcile "$MOUNT" 2>/dev/null || true
  run_vm forceclose "$MOUNT" 2>/dev/null || true
else
  echo "    warn: vault_manager not found at $VM"
fi
if command -v cryptsetup >/dev/null 2>&1; then
  sudo cryptsetup close digitalvault 2>/dev/null || cryptsetup close digitalvault 2>/dev/null || true
fi

echo "==> [2/4] Loop containers under $DATA/containers"
mkdir -p "$DATA/containers"
if command -v losetup >/dev/null 2>&1; then
  while IFS= read -r line; do
    dev="${line%%:*}"
    if [[ "$line" == *"$DATA/containers"* ]]; then
      echo "    detach $dev"
      sudo losetup -d "$dev" 2>/dev/null || losetup -d "$dev" 2>/dev/null || true
    fi
  done < <(losetup -a 2>/dev/null || true)
fi
shopt -s nullglob
imgs=("$DATA/containers/"*.img)
if [[ ${#imgs[@]} -gt 0 ]]; then
  echo "    remove ${#imgs[@]} image(s)"
  rm -f "${imgs[@]}"
fi
shopt -u nullglob

echo "==> [3/4] Registry, state, GUI cache"
rm -f "$DATA/vault_registry.json" "$DATA/vault_state.json"
if [[ -f "$CONF" ]]; then
  sed -i '/sensor\/occupiedSlots/d' "$CONF" 2>/dev/null || true
fi
rm -f "$ROOT/security_logs.json"
rm -f "$ROOT/hw_simulator/.fingerprint_db.json" "$ROOT/hw_simulator/.keystore.json" 2>/dev/null || true

if [[ "$SKIP_SENSOR" -eq 1 ]]; then
  echo "==> [4/4] Sensor wipe skipped (--skip-sensor)"
else
  echo "==> [4/4] Wipe fingerprint sensor ($HAL_PATH)"
  if pgrep -x DigitalVaultGUI >/dev/null 2>&1; then
    echo "    error: close DigitalVaultGUI first (serial port must be free)" >&2
    exit 1
  fi
  if [[ ! -e "$SERIAL_PORT" ]]; then
    echo "    warn: $SERIAL_PORT not found — skipping sensor wipe" >&2
  elif [[ ! -x "$TEST_HAL" ]]; then
    echo "    warn: build test_hal first: cmake --build $BUILD --target test_hal" >&2
  else
    "$TEST_HAL" "$HAL_PATH" --wipe-sensor || {
      echo "    warn: sensor wipe returned non-zero (check cable / firmware)" >&2
    }
  fi
fi

echo ""
echo "Done. Verify:"
echo "  ls -la $DATA"
echo "  losetup -a | grep DigitalVault || echo '(no DV loops)'"
if [[ "$SKIP_SENSOR" -eq 0 ]] && [[ -x "$TEST_HAL" ]] && [[ -e "$SERIAL_PORT" ]]; then
  echo "  $TEST_HAL $HAL_PATH  # then 'p' for ~0 templates"
fi
