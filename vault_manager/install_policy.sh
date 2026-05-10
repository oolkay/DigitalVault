#!/bin/bash
# DigitalVault — PolicyKit Policy Installer
#
# Bu script, vault_manager binary'si için PolicyKit policy dosyasını
# sisteme kurar. Kurulduktan sonra pkexec ile grafik şifre diyaloğu
# üzerinden vault_manager çalıştırılabilir.
#
# Kullanım:
#   sudo ./install_policy.sh [vault_manager_path]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
POLICY_SRC="${SCRIPT_DIR}/com.digitalvault.manager.policy"
POLICY_DST="/usr/share/polkit-1/actions/com.digitalvault.manager.policy"

# vault_manager binary yolunu belirle
if [ -n "$1" ]; then
    VM_PATH="$(realpath "$1")"
elif [ -f "${SCRIPT_DIR}/../build/bin/vault_manager" ]; then
    VM_PATH="$(realpath "${SCRIPT_DIR}/../build/bin/vault_manager")"
elif [ -f "${SCRIPT_DIR}/vault_manager" ]; then
    VM_PATH="$(realpath "${SCRIPT_DIR}/vault_manager")"
else
    echo "HATA: vault_manager binary bulunamadı!"
    echo "Kullanım: sudo $0 [vault_manager_path]"
    exit 1
fi

echo "╔══════════════════════════════════════════════════════╗"
echo "║  DigitalVault — PolicyKit Policy Installer           ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "  vault_manager : ${VM_PATH}"
echo "  Policy hedef  : ${POLICY_DST}"
echo ""

# Dosya var mı kontrol et
if [ ! -f "${POLICY_SRC}" ]; then
    echo "HATA: Policy template bulunamadı: ${POLICY_SRC}"
    exit 1
fi

if [ ! -f "${VM_PATH}" ]; then
    echo "HATA: vault_manager binary bulunamadı: ${VM_PATH}"
    exit 1
fi

# Policy dosyasını kopyala ve yolu yerleştir
sed "s|@VAULT_MANAGER_PATH@|${VM_PATH}|g" "${POLICY_SRC}" > "${POLICY_DST}"
chmod 644 "${POLICY_DST}"

echo "✓ PolicyKit policy kuruldu!"
echo ""
echo "Artık GUI üzerinden kasa açma/kapama işlemleri"
echo "grafik şifre diyaloğu ile yapılabilir."
echo ""
