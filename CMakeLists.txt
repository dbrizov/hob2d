cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set project name.
project(hob2d)

# Use vcpkg to find packages.
if (CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif ()

# Find all C++ source files recursively.
file(GLOB_RECURSE CPP_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp)

# Find all header files recursively.
file(GLOB_RECURSE HEADER_FILES ${CMAKE_SOURCE_DIR}/src/*.h)

# Set the executable.
add_executable(${PROJECT_NAME} ${CPP_FILES} ${HEADER_FILES})

# Since the headers are also in /src we are adding it as an include directory.
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Place the executable and its synced files in a per-config subfolder, ready to distribute.
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

# Sync the assets/ folder next to the executable after every build.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMAND ${CMAKE_COMMAND} -E echo "Synced assets to $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
)

# Sync the config/ folder next to the executable after every build.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/config" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
        COMMAND ${CMAKE_COMMAND} -E echo "Synced config to $<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
)

# Sync the scripts/ folder next to the executable after every build.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/scripts" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
        COMMAND ${CMAKE_COMMAND} -E echo "Synced scripts to $<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
)

# Copy the (redistributable, Release) MSVC C++ runtime DLLs next to the executable.
if (MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE) # we copy manually, no install() rule
    include(InstallRequiredSystemLibraries)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
            COMMAND_EXPAND_LISTS
            COMMAND ${CMAKE_COMMAND} -E echo "Copied MSVC runtime DLLs next to $<TARGET_FILE_DIR:${PROJECT_NAME}>")
endif ()

# On macOS, DXC ships as libdxcompiler.dylib with install name
# @rpath/libdxcompiler.dylib (provided by the arm64-osx directx-dxc overlay port).
# Add @executable_path to the rpath and copy the dylib next to the executable so
# the runtime loader can resolve it both in dev and when the binary is relocated.
if (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "@executable_path"
            INSTALL_RPATH "@executable_path")

    find_file(DXCOMPILER_DYLIB
            NAMES libdxcompiler.dylib
            PATHS "${CMAKE_PREFIX_PATH}" "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}"
            PATH_SUFFIXES lib
            NO_DEFAULT_PATH)

    if (DXCOMPILER_DYLIB)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXCOMPILER_DYLIB}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
                COMMAND ${CMAKE_COMMAND} -E echo "Copied libdxcompiler.dylib next to $<TARGET_FILE_DIR:${PROJECT_NAME}>")
    else ()
        message(WARNING "libdxcompiler.dylib not found; runtime HLSL compilation will fail until it is on the loader path.")
    endif ()
endif ()

# Find and link nlohmann-json.
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# Find and link SDL3.
find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)

# Find and link SDL3_image.
find_package(SDL3_image CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_image::SDL3_image)

# Find and link SDL3_ttf.
find_package(SDL3_ttf CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_ttf::SDL3_ttf)

# Find and link SDL3_shadercross.
find_package(SDL3_shadercross CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_shadercross::SDL3_shadercross)

# Find and link SPIRV-Reflect.
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::spirv-reflect)

# Find and link box2d.
find_package(box2d CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE box2d::box2d)

# Find and link imgui.
find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)

# Find and link RmlUi.
find_package(RmlUi CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE RmlUi::Core)

# Find and link Lua + sol2.
find_package(Lua REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LUA_LIBRARIES})

find_package(sol2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE sol2::sol2)

# sol2's template-heavy bindings exceed MSVC's default COFF section limit.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif ()

# Force-include <cstdint> into every translation unit.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FIcstdint)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -include cstdint)
endif ()

# Precompiled header.
target_precompile_headers(${PROJECT_NAME} PRIVATE
        # Standard library — slow parsers first.
        <filesystem>
        <format>
        <algorithm>
        <functional>
        <memory>
        <optional>
        <string>
        <string_view>
        <utility>
        <vector>
        <unordered_map>
        <unordered_set>
        <cstdint>

        # Third party libraries.
        <SDL3/SDL.h>
        <sol/sol.hpp>
)
