cmake_minimum_required(VERSION 3.31)
set(PROJECT_NAME "fw")
project(${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

if (POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif ()

FetchContent_Declare(
        fmt
        GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
        GIT_TAG "e424e3f2e607da02742f73db84873b8084fc714c" # 12.0.0
        GIT_SHALLOW TRUE
        CXX_STANDARD 17
)
FetchContent_MakeAvailable(fmt)
target_compile_options(fmt PRIVATE "-w")

FetchContent_Declare(
        minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG c3fcafdc10146beb5919319d0683e44e3c30d537
        GIT_SHALLOW TRUE
)
FetchContent_GetProperties(minhook)
if (NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif ()

add_library(minhook STATIC
        ${minhook_SOURCE_DIR}/src/buffer.c
        ${minhook_SOURCE_DIR}/src/hook.c
        ${minhook_SOURCE_DIR}/src/trampoline.c
        ${minhook_SOURCE_DIR}/src/hde/hde64.c
)

target_include_directories(minhook PUBLIC
        ${minhook_SOURCE_DIR}/include
)

FetchContent_Declare(
        json
        GIT_REPOSITORY "https://github.com/nlohmann/json.git"
        GIT_TAG "55f93686c01528224f448c19128836e7df245f72" # 3.12.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
        zydis
        GIT_REPOSITORY "https://github.com/zyantific/zydis.git"
        GIT_TAG "a2278f1d254e492f6a6b39f6cb5d1f5d515659dc" # 4.1.1
)
FetchContent_MakeAvailable(zydis)

add_library(${PROJECT_NAME} STATIC
        src/logger.cpp
        src/proxy.cpp
        src/settings.cpp
        src/util.cpp

        src/memory/handle.cpp
        src/memory/hook.cpp
        src/memory/memory.cpp
        src/memory/module.cpp
        src/memory/pattern.cpp
        src/memory/pointer_validator.cpp
        src/memory/protection.cpp
        src/memory/range.cpp
        src/memory/scanner.cpp

        src/memory/hooks/d3d11.cpp
        src/memory/hooks/wndproc.cpp
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${fmt_SOURCE_DIR}/include
        ${minhook_SOURCE_DIR}/include
        ${json_SOURCE_DIR}/single_include
        ${zydis_SOURCE_DIR}/include
        ${zydis_SOURCE_DIR}/dependencies/zycore/include
)

target_precompile_headers(${PROJECT_NAME}
        PUBLIC
        src/pch.hpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        minhook
        fmt
        d3d11
        dxgi
        d3dcompiler
        xinput
        gdi32
        user32
        dwmapi
        Zydis
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)