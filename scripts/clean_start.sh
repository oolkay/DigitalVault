#!/usr/bin/env bash
# Clean DigitalVault GUI registry/state and detach GUI-created loop devices.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${DIGITALVAULT_BUILD:-$ROOT/build}"
VM="$BUILD/bin/vault_manager"
MOUNT="${VAULT_MOUNT:-$HOME/VaultMount}"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}/DigitalVault"
REGISTRY="$DATA/vault_registry.json"
STATE="$DATA/vault_state.json"
CONTAINERS="$DATA/containers"
USE_PKEXEC=1
DELETE_IMAGES=0
DRY_RUN=0

usage() {
  cat <<EOF
Usage: $0 [options]

Clean start for GUI tests:
  - close/reconcile DigitalVault runtime if vault_manager is built
  - detach loop devices created from DigitalVault container images
  - remove GUI registry/state files

Options:
  --delete-images   Also delete container image files under $CONTAINERS
  --no-pkexec       Run vault_manager without pkexec (useful when already root)
  --dry-run         Print actions without changing anything
  -h, --help        Show this help

Environment:
  DIGITALVAULT_BUILD  Build directory (default: $ROOT/build)
  VAULT_MOUNT         Mount point (default: ~/VaultMount)
  XDG_DATA_HOME       Data root override (default: ~/.local/share)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --delete-images) DELETE_IMAGES=1; shift ;;
    --no-pkexec) USE_PKEXEC=0; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
done

say() {
  printf '%s\n' "$*"
}

run_or_print() {
  if [[ "$DRY_RUN" -eq 1 ]]; then
    printf 'dry-run:'
    printf ' %q' "$@"
    printf '\n'
  else
    "$@"
  fi
}

run_vm() {
  if [[ ! -x "$VM" ]]; then
    say "warn: vault_manager not found at $VM"
    return 0
  fi

  if [[ "$USE_PKEXEC" -eq 1 && "$(id -u)" -ne 0 ]]; then
    run_or_print pkexec "$VM" "$@"
  else
    run_or_print "$VM" "$@"
  fi
}

is_digitalvault_backing() {
  local backing="$1"
  [[ "$backing" == "$CONTAINERS"/*.img ]]
}

detach_digitalvault_loops() {
  if ! command -v losetup >/dev/null 2>&1; then
    say "warn: losetup not found; skipping loop cleanup"
    return 0
  fi

  local found=0
  while IFS= read -r line; do
    local dev="${line%%:*}"
    local backing=""

    if [[ "$line" =~ \((.*)\)$ ]]; then
      backing="${BASH_REMATCH[1]}"
    fi

    if [[ -n "$backing" ]] && is_digitalvault_backing "$backing"; then
      found=1
      say "detach: $dev ($backing)"
      if [[ "$DRY_RUN" -eq 1 ]]; then
        say "dry-run: losetup -d $dev"
      elif [[ "$(id -u)" -eq 0 ]]; then
        losetup -d "$dev" 2>/dev/null || say "warn: could not detach $dev"
      else
        sudo losetup -d "$dev" 2>/dev/null || losetup -d "$dev" 2>/dev/null || say "warn: could not detach $dev"
      fi
    fi
  done < <(losetup -a 2>/dev/null || true)

  if [[ "$found" -eq 0 ]]; then
    say "loops: no DigitalVault container loops found"
  fi
}

delete_registry_files() {
  say "registry: $REGISTRY"
  say "state:    $STATE"
  if [[ "$DRY_RUN" -eq 1 ]]; then
    say "dry-run: rm -f $REGISTRY $STATE"
  else
    rm -f "$REGISTRY" "$STATE"
  fi
}

delete_container_images() {
  if [[ "$DELETE_IMAGES" -ne 1 ]]; then
    return 0
  fi

  if [[ ! -d "$CONTAINERS" ]]; then
    say "images: $CONTAINERS does not exist"
    return 0
  fi

  shopt -s nullglob
  local images=("$CONTAINERS"/*.img)
  shopt -u nullglob

  if [[ "${#images[@]}" -eq 0 ]]; then
    say "images: no container images found"
    return 0
  fi

  say "images: deleting ${#images[@]} file(s)"
  if [[ "$DRY_RUN" -eq 1 ]]; then
    printf 'dry-run: rm -f'
    printf ' %q' "${images[@]}"
    printf '\n'
  else
    rm -f "${images[@]}"
  fi
}

say "==> DigitalVault clean start"
say "data: $DATA"

say "==> Closing vault runtime"
run_vm reconcile "$MOUNT" 2>/dev/null || true
run_vm forceclose "$MOUNT" 2>/dev/null || true

say "==> Detaching GUI-created loop devices"
detach_digitalvault_loops

say "==> Clearing GUI registry/state"
delete_registry_files

delete_container_images

say "done"
