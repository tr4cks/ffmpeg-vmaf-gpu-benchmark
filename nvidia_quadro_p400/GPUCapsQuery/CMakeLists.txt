cmake_minimum_required(VERSION 3.18)

project(GPUCapsQuery LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Directory structure (this file is in: <SDK_ROOT>/Samples/GPUCapsQuery)
set(SAMPLES_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
set(SDK_ROOT     ${CMAKE_CURRENT_LIST_DIR}/../..)

# CUDA (link required)
find_package(CUDAToolkit REQUIRED)

# Local sources
file(GLOB LOCAL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/*.c
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

set(NVENCODER_SRC ${SAMPLES_DIR}/NvCodec/NvEncoder/NvEncoder.cpp)

add_executable(GPUCapsQuery
    ${LOCAL_SOURCES}
    ${NVENCODER_SRC}
)

# Required include directories
# - SDK_ROOT/Interface for nvEncodeAPI.h
# - SAMPLES_DIR/NvCodec (parent) to allow #include <NvEncoder/NvEncoder.h>
target_include_directories(GPUCapsQuery PRIVATE
    ${SDK_ROOT}/Interface
    ${SAMPLES_DIR}/NvCodec
    ${SAMPLES_DIR}/Utils
)

# Determine NVENC library according to OS/arch
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(NVENC_LIB ${SDK_ROOT}/Lib/x64/nvencodeapi.lib)
    else()
        set(NVENC_LIB ${SDK_ROOT}/Lib/Win32/nvencodeapi.lib)
    endif()
elseif(UNIX)
    # Detect architecture for Linux stubs
    set(_ARCH_DIR "x86_64")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(_ARCH_DIR "aarch64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
        set(_ARCH_DIR "ppc64le")
    endif()
    set(NVENC_LIB ${SDK_ROOT}/Lib/linux/stubs/${_ARCH_DIR}/libnvidia-encode.so)
else()
    message(FATAL_ERROR "Unsupported platform for NVENC")
endif()

# Link required libraries
target_link_libraries(GPUCapsQuery PRIVATE
    CUDA::cuda_driver     # link to CUDA driver
    ${NVENC_LIB}          # NVENC
)

# Info messages
message(STATUS "SDK root:       ${SDK_ROOT}")
message(STATUS "Samples dir:    ${SAMPLES_DIR}")
message(STATUS "NVENC library:  ${NVENC_LIB}")
