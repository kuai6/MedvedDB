# usage:
# cmake -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
# cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt
# cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo CMakeLists.txt
# cmake -DCMAKE_BUILD_TYPE=MinSizeRel CMakeLists.txt
# cmake -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles" .

# mkdir build
# cd build>
# cmake -DCMAKE_INSTALL_PREFIX=/opt ..
# cpack ..

cmake_minimum_required(VERSION 3.2)
project(MedvedDB)
cmake_policy(SET CMP0015 NEW)

SET(MAJOR_VERSION 1)
SET(MINOR_VERSION 0)
SET(PATCH_VERSION 0)

set(VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION})

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mdv_thirdparty)


#*********************************************************
# set build type
#*********************************************************
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()


#*********************************************************
# determine platform
#*********************************************************
set(PLATFORM UNKNOWN)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(PLATFORM LINUX)
    if (DEFINED ${ANDROID_PLATFORM})
        set(PLATFORM ANDROID)
        add_definitions(-DANDROID_PLATFORM)
    endif()
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(PLATFORM MAC_OS)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(PLATFORM WINDOWS)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(PLATFORM ANDROID)
    add_definitions(-DANDROID_PLATFORM)
endif()

message("Platform: " ${CMAKE_SYSTEM_NAME} "-" ${PLATFORM})


#*********************************************************
# BITNESS
#*********************************************************
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BITNESS 64)
    add_definitions (-DMEDVED_X64)
    message("Bitness: 64 bit")
else()
    set(BITNESS 32)
    add_definitions (-DMEDVED_X86)
    message("Bitness: 32 bit")
endif()


#*********************************************************
# binary directory
#*********************************************************
if (NOT BIN_DIR)
    set(BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()


#*********************************************************
# useful macros
#*********************************************************
macro(use_c11)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endmacro(use_c11)


include(cmake/doxygen.cmake)

add_subdirectory(${ROOT_DIR}/mdv_thirdparty)
add_subdirectory(${ROOT_DIR}/mdv_platform)
add_subdirectory(${ROOT_DIR}/mdv_types)
add_subdirectory(${ROOT_DIR}/mdv_core)
add_subdirectory(${ROOT_DIR}/mdv_service)
add_subdirectory(${ROOT_DIR}/mdv_api)
add_subdirectory(${ROOT_DIR}/mdv_client)
add_subdirectory(${ROOT_DIR}/mdv_tests)

set(DOXY_INPUT
    ${ROOT_DIR}/mdv_api
    ${ROOT_DIR}/mdv_client
    ${ROOT_DIR}/mdv_core
    ${ROOT_DIR}/mdv_platform
    ${ROOT_DIR}/mdv_service
    ${ROOT_DIR}/mdv_types
)

set(DOXY_OUTPUT_DIRECTORY "${BIN_DIR}/doc")

add_documentation(doc Doxyfile)

