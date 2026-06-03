#!/bin/bash
# DigitalVault — PolicyKit Policy Installer
#
# Bu script, vault_manager binary'si için PolicyKit policy dosyasını
# sisteme kurar. Kurulduktan sonra pkexec ile grafik şifre diyaloğu
# üzerinden vault_manager çalıştırılabilir.
#
# Kullanım:
#   sudo ./install_policy.sh [vault_manager_path] [allowed_user]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
POLICY_SRC="${SCRIPT_DIR}/com.digitalvault.manager.policy"
POLICY_DST="/usr/share/polkit-1/actions/com.digitalvault.manager.policy"
RULES_SRC="${SCRIPT_DIR}/49-digitalvault-manager.rules"
RULES_DST="/etc/polkit-1/rules.d/49-digitalvault-manager.rules"
PKLA_SRC="${SCRIPT_DIR}/49-digitalvault-manager.pkla"
PKLA_DST="/etc/polkit-1/localauthority/50-local.d/49-digitalvault-manager.pkla"

# vault_manager binary yolunu belirle
if [ -n "$1" ]; then
    VM_PATH="$(realpath "$1")"
elif [ -f "${SCRIPT_DIR}/../build/bin/vault_manager" ]; then
    VM_PATH="$(realpath "${SCRIPT_DIR}/../build/bin/vault_manager")"
elif [ -f "${SCRIPT_DIR}/vault_manager" ]; then
    VM_PATH="$(realpath "${SCRIPT_DIR}/vault_manager")"
else
    echo "HATA: vault_manager binary bulunamadı!"
    echo "Kullanım: sudo $0 [vault_manager_path] [allowed_user]"
    exit 1
fi

if [ -n "$2" ]; then
    ALLOWED_USER="$2"
elif [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER}" != "root" ]; then
    ALLOWED_USER="${SUDO_USER}"
else
    ALLOWED_USER="$(id -un)"
fi

echo "╔══════════════════════════════════════════════════════╗"
echo "║  DigitalVault — PolicyKit Policy Installer           ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "  vault_manager : ${VM_PATH}"
echo "  Policy hedef  : ${POLICY_DST}"
echo "  Rules hedef   : ${RULES_DST}"
echo "  PKLA hedef    : ${PKLA_DST}"
echo "  Şifresiz user : ${ALLOWED_USER}"
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

if [ ! -f "${RULES_SRC}" ]; then
    echo "HATA: PolicyKit rules template bulunamadı: ${RULES_SRC}"
    exit 1
fi

if [ ! -f "${PKLA_SRC}" ]; then
    echo "HATA: PolicyKit pkla template bulunamadı: ${PKLA_SRC}"
    exit 1
fi

# Policy dosyasını kopyala ve yolu yerleştir
sed "s|@VAULT_MANAGER_PATH@|${VM_PATH}|g" "${POLICY_SRC}" > "${POLICY_DST}"
chown root:root "${POLICY_DST}"
chmod 644 "${POLICY_DST}"

mkdir -p "$(dirname "${RULES_DST}")"
sed -e "s|@DIGITALVAULT_USER@|${ALLOWED_USER}|g" \
    -e "s|@VAULT_MANAGER_PATH@|${VM_PATH}|g" \
    "${RULES_SRC}" > "${RULES_DST}"
chown root:root "${RULES_DST}"
chmod 644 "${RULES_DST}"

mkdir -p "$(dirname "${PKLA_DST}")"
sed "s|@DIGITALVAULT_USER@|${ALLOWED_USER}|g" "${PKLA_SRC}" > "${PKLA_DST}"
chown root:root "${PKLA_DST}"
chmod 644 "${PKLA_DST}"

echo "✓ PolicyKit policy kuruldu!"
echo "✓ Şifresiz PolicyKit rule kuruldu!"
echo "✓ Şifresiz PolicyKit pkla fallback kuruldu!"
echo ""
echo "Artık GUI üzerinden kasa açma/kapama işlemleri"
echo "şifre istemeden yapılabilir."
echo ""
echo "  vault_manager createloop — Create New loop device (truncate + losetup)"
echo "  Aynı PolicyKit action (com.digitalvault.manager.run) ile çalışır."
echo ""
echo "Create New (loop device) ve Setup Vault işlemleri de"
echo "aynı vault_manager policy üzerinden pkexec ile çalışır"
echo "(createloop, setup, open, close)."
echo ""
