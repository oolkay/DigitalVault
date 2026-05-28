cmake_minimum_required(VERSION 3.16)

add_library(hal STATIC
    hal_core.c
    hal_simulator.c
    hal_serial.c
    hal_protocol.c
    hal_common.c
)
target_include_directories(hal PUBLIC .)
target_link_libraries(hal PRIVATE ${CJSON_LIBRARIES} OpenSSL::Crypto)
target_compile_options(hal PRIVATE -Wall -Wextra)

add_executable(test_hal test_hal.c)
target_link_libraries(test_hal PRIVATE hal ${CJSON_LIBRARIES} OpenSSL::Crypto)
target_compile_options(test_hal PRIVATE -Wall -Wextra)
set_target_properties(test_hal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
