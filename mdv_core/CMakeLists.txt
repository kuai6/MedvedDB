cmake_minimum_required(VERSION 3.2)
project(mdv_core)

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

use_c11()

include_directories(
    ${LIBS_DIR}/binn
    ${LIBS_DIR}/inih
    ${LIBS_DIR}/zf_log
    ${LIBS_DIR}/lmdb
    ${ROOT_DIR}/mdv_platform
    ${ROOT_DIR}/mdv_api
    ${ROOT_DIR}/mdv_types
)

add_library(mdv_core STATIC ${SOURCES})

add_dependencies(mdv_core inih mdv_platform mdv_api mdv_types lmdb)

target_link_libraries(mdv_core inih zf_log mdv_platform mdv_api mdv_types lmdb)
