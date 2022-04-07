cmake_minimum_required(VERSION 3.10)
include(GNUInstallDirs)

project(vivid-sm
    VERSION 0.1.0)

option(VIVID_LOCKFREE      "Enable use of atomic operations" OFF)
option(VIVID_LOG           "Enable logging" ON)
option(VIVID_UML           "Enable UML generation" ON)
option(VIVID_PARAM         "Enable parametrized events" ON)
option(VIVID_PARAM_DYNAMIC "Enable dynamically allocated parameters" OFF)
option(VIVID_EXAMPLES      "Enable examples" OFF)

option(VIVID_BINDING_LIBEV    "Enable binding for libev" OFF)
option(VIVID_BINDING_WIN32    "Enable binding for Windows" OFF)
option(VIVID_BINDING_LINUX    "Enable binding for Linux" OFF)
option(VIVID_BINDING_FREERTOS "Enable binding for FreeRTOS" OFF)

configure_file(vivid-sm.pc.in vivid-sm.pc @ONLY)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -DVIVID_LOCKFREE=$<BOOL:${VIVID_LOCKFREE}>
    -DVIVID_LOG=$<BOOL:${VIVID_LOG}>
    -DVIVID_UML=$<BOOL:${VIVID_UML}>
    -DVIVID_PARAM=$<BOOL:${VIVID_PARAM}>
    -DVIVID_PARAM_DYNAMIC=$<BOOL:${VIVID_PARAM_DYNAMIC}>
)

include_directories(
    inc
)

if(VIVID_EXAMPLES)
    if(VIVID_BINDING_FREERTOS)
        include(examples/platform/freertos/freertos-example.cmake)
    endif()
    add_subdirectory(examples)
endif()

add_subdirectory(src)
