cmake_minimum_required(VERSION 3.20)
project(links_awakening_ps5_core C CXX)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(NOT PS5 OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    message(FATAL_ERROR "Use the PS5 native toolchain; this target is a console cross-build")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(PS5_SDL3_SOURCE "${ROOT}/../Dusklight-PS5/.deps/src/SDL" CACHE PATH "Prepared native SDL3 source")
file(GLOB CORE CONFIGURE_DEPENDS
    "${ROOT}/runtime/*.c" "${ROOT}/runtime/hle/*.c"
    "${ROOT}/runtime/services/*.c" "${ROOT}/runtime/nvn/*.c"
    "${ROOT}/runtime/nvn/maxwell/*.c")
# Compile shared runtime separately from entry, UI, platform and private game code.
list(FILTER CORE EXCLUDE REGEX "/runtime/main\\.c$")
list(FILTER CORE EXCLUDE REGEX "/hle/movie\\.c$")
list(FILTER CORE EXCLUDE REGEX "/nvn/smaa_data\\.c$")
add_library(la_ps5_core STATIC ${CORE})
set(PS5_PLATFORM_SOURCE "${ROOT}/../PS5_PayloadSDK/platform" CACHE PATH "Prepared PS5 platform library source")
add_library(la_ps5_memory STATIC platform/guest_memory.c)
target_include_directories(la_ps5_memory PRIVATE "${ROOT}/runtime" "${PS5_PLATFORM_SOURCE}/include")
target_compile_options(la_ps5_memory PRIVATE -O2 -Wall -Wextra -Werror)
add_library(la_ps5_entry STATIC "${ROOT}/runtime/main.c")
target_compile_definitions(la_ps5_entry PRIVATE main=la_game_main)
target_include_directories(la_ps5_entry PRIVATE "${ROOT}/runtime" "${PS5_SDL3_SOURCE}/include")
target_include_directories(la_ps5_entry PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/module")
target_compile_options(la_ps5_entry PRIVATE -O2 -fno-strict-aliasing)
target_include_directories(la_ps5_core PRIVATE
    "${ROOT}/runtime" "${ROOT}/runtime/nvn" "${PS5_SDL3_SOURCE}/include" "${CMAKE_CURRENT_SOURCE_DIR}/module")
target_compile_options(la_ps5_core PRIVATE -O2 -fno-strict-aliasing -Wall -Wno-unused-function)

option(LA_COMPILE_VULKAN "Compile the native Vulkan renderer" OFF)
if(LA_COMPILE_VULKAN)
    set(PS5_VULKAN_HEADERS "${ROOT}/../Dusklight-PS5/.deps/src/mesa/include" CACHE PATH "Prepared Vulkan headers")
    file(GLOB VULKAN_SRC CONFIGURE_DEPENDS "${ROOT}/runtime/nvn/vulkan/*.c" "${ROOT}/runtime/nvn/vulkan/*.cpp")
    add_library(la_ps5_vulkan STATIC ${VULKAN_SRC} platform/vulkan_surface.c third_party/volk/volk.c)
    target_include_directories(la_ps5_vulkan PRIVATE
        "${ROOT}/runtime" "${ROOT}/runtime/nvn" "${PS5_SDL3_SOURCE}/include" "${CMAKE_CURRENT_SOURCE_DIR}/platform"
        "${PS5_VULKAN_HEADERS}" "${ROOT}/.deps/src/VulkanMemoryAllocator/include"
        "${ROOT}/.deps/src/shaderc/libshaderc/include"
        "${ROOT}/third_party/imgui" "${ROOT}/third_party/imgui/backends")
    target_include_directories(la_ps5_vulkan PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/volk")
    target_compile_definitions(la_ps5_vulkan PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
    target_compile_options(la_ps5_vulkan PRIVATE -O2 -fno-strict-aliasing -include volk.h)
endif()

option(LA_COMPILE_UI "Compile native ImGui menus and launcher" ON)
if(LA_COMPILE_UI)
    file(GLOB UI_SRC CONFIGURE_DEPENDS "${ROOT}/runtime/ui/*.mm")
    set_source_files_properties(${UI_SRC} PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x c++")
    add_library(la_ps5_ui STATIC ${UI_SRC}
        "${ROOT}/third_party/imgui/imgui.cpp" "${ROOT}/third_party/imgui/imgui_draw.cpp"
        "${ROOT}/third_party/imgui/imgui_tables.cpp" "${ROOT}/third_party/imgui/imgui_widgets.cpp"
        "${ROOT}/third_party/imgui/backends/imgui_impl_sdl3.cpp"
        "${ROOT}/third_party/imgui/backends/imgui_impl_vulkan.cpp")
    target_include_directories(la_ps5_ui PRIVATE "${ROOT}/runtime" "${PS5_SDL3_SOURCE}/include"
        "${ROOT}/third_party/imgui" "${ROOT}/third_party/imgui/backends" "${PS5_VULKAN_HEADERS}"
        "${ROOT}/build/ps5-ffmpeg/install/include" "${CMAKE_CURRENT_SOURCE_DIR}/module")
    target_include_directories(la_ps5_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/volk")
    target_compile_definitions(la_ps5_ui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
    target_compile_options(la_ps5_ui PRIVATE -O2 -fno-strict-aliasing -include volk.h)
endif()

add_library(la_ps5_platform STATIC platform/process.c platform/main.c platform/filesystem.c platform/input.cpp native/platform.c)
target_include_directories(la_ps5_platform PRIVATE "${ROOT}/runtime" "${PS5_PLATFORM_SOURCE}/include" "${PS5_SDL3_SOURCE}/include")
target_compile_options(la_ps5_platform PRIVATE -O2 -Wall -Wextra -Werror)

# The public app contains only a hardened loader and the small runtime ABI bridge.
# Generated game functions and their tables are supplied later by the user's module.
set(MODULE_GEN "${CMAKE_CURRENT_BINARY_DIR}/module")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/module-support.py"
    --output "${MODULE_GEN}" COMMAND_ERROR_IS_FATAL ANY)
add_library(la_ps5_module STATIC module/loader.cpp module/bind.cpp "${MODULE_GEN}/exports.cpp")
target_include_directories(la_ps5_module PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/module" PRIVATE
    "${MODULE_GEN}" "${ROOT}/runtime")
target_compile_options(la_ps5_module PRIVATE -O2 -Wall -Wextra -Werror)

if(EXISTS "${ROOT}/build/ps5-ffmpeg/install/include/libavcodec/avcodec.h")
    target_sources(la_ps5_core PRIVATE "${ROOT}/runtime/hle/movie.c")
    target_include_directories(la_ps5_core PRIVATE "${ROOT}/build/ps5-ffmpeg/install/include")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-smaa.py"
    "${ROOT}/third_party/smaa/SMAA.hlsl" "${CMAKE_CURRENT_BINARY_DIR}/smaa_hlsl.inc"
    COMMAND_ERROR_IS_FATAL ANY)
target_sources(la_ps5_core PRIVATE "${ROOT}/runtime/nvn/smaa_data.c")
target_include_directories(la_ps5_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${ROOT}/third_party/smaa/Textures")
