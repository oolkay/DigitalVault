cmake_minimum_required(VERSION 3.16)

add_library(hal STATIC 
    hal_simulator.c 
    hal_common.c
)
target_include_directories(hal PUBLIC .)
target_link_libraries(hal PRIVATE ${CJSON_LIBRARIES} OpenSSL::Crypto)
target_compile_options(hal PRIVATE -Wall -Wextra)
