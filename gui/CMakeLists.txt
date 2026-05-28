cmake_minimum_required(VERSION 3.16)

project(DigitalVaultGUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Concurrent)

set(PROJECT_SOURCES
    main.cpp
    vault_controller.h
    vault_controller.cpp
    serial_bridge.h
    serial_bridge.cpp
)

qt_add_executable(DigitalVaultGUI
    ${PROJECT_SOURCES}
)

target_link_libraries(DigitalVaultGUI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Concurrent
)

target_include_directories(DigitalVaultGUI PRIVATE .)

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(DigitalVaultGUI
    URI DigitalVault
    VERSION 1.0
    QML_FILES
        main.qml
        Dashboard.qml
        CreateVault.qml
        VaultManager.qml
        VaultManagerNavBar.qml
        VaultManagerHome.qml
        VaultManagerAddVault.qml
        VaultManagerVaultDetail.qml
        ManagedDeviceRow.qml
        DiscoveredStorageRow.qml
        PartitionLayoutView.qml
        PartitionLayoutTable.qml
        PartitionListRow.qml
        StripeFill.qml
        StorageOutcomePreview.qml
        StorageActionPanel.qml
        VaultListItem.qml
        VaultDetailPanel.qml
        VaultStatusBadge.qml
        ModalScrim.qml
        SetupProgressOverlay.qml
        FingerprintWizard.qml
        AccessManager.qml
        FingerprintManager.qml
        SecurityLog.qml
        Settings.qml
        Theme.qml
        WizardChrome.qml
        WizardStepTitle.qml
        WizardFieldLabel.qml
        WizardTextField.qml
        WizardSizeSlider.qml
        WizardPartitionFields.qml
        WizardSummaryBox.qml
        WizardActionButton.qml
    SOURCES
        vault_controller.h
        vault_controller.cpp
)

set_target_properties(DigitalVaultGUI PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
