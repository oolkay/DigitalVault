cmake_minimum_required(VERSION 3.16)
project(DigitalVault VERSION 2.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Dependencies ──────────────────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(CJSON REQUIRED libcjson)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 Network REQUIRED)

# ─── Subdirectories ────────────────────────────────────────────────────────

add_subdirectory(hal)
add_subdirectory(vault_manager)
add_subdirectory(gui)

# ─── Status ────────────────────────────────────────────────────────────────

message(STATUS "DigitalVault v2.0 — LUKS + ext4 Architecture")
message(STATUS "  - Vault Manager : bin/vault_manager")
message(STATUS "  - GUI binary    : bin/DigitalVaultGUI")
