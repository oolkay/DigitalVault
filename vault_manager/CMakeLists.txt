cmake_minimum_required(VERSION 3.16)

# ─── Vault Manager Binary ─────────────────────────────────────────────────
# LUKS + ext4 tabanlı şifreli disk yöneticisi.
# HAL ile biyometrik doğrulama yaparak cryptsetup'ı kontrol eder.

add_executable(vault_manager vault_manager.c)

target_link_libraries(vault_manager PRIVATE
    hal                     # Biyometrik doğrulama (HAL)
    ${CJSON_LIBRARIES}      # HAL'ın bağımlılığı (JSON protocol)
)

target_compile_options(vault_manager PRIVATE -Wall -Wextra -Wpedantic)

# Binary'yi ortak çıktı dizinine koy
set_target_properties(vault_manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
