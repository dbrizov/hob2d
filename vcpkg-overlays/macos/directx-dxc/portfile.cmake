# arm64-osx overlay for directx-dxc.
#
# Microsoft does not publish prebuilt DirectX Shader Compiler binaries for
# macOS. The arm64 build (libdxcompiler.dylib, libdxil.dylib, dxc tool, the
# public headers, and the license) is vendored under assets/ so the port has no
# external dependencies: the binaries originate from upstream DXC v1.9.2602
# (release date 2026-02-20), repackaged by MethanePowered/DirectXShaderCompilerBinary.
#
# This installs them in the layout expected by directx-dxc-config.cmake.in
# (imported Microsoft::* targets plus the dxc command-line tool).
set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)

if(NOT (VCPKG_TARGET_IS_OSX AND VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64"))
    message(FATAL_ERROR "${PORT} overlay only supports arm64-osx.")
endif()

set(ASSETS "${CMAKE_CURRENT_LIST_DIR}/assets")

vcpkg_extract_source_archive(
    SOURCE_PATH
    ARCHIVE "${ASSETS}/MacOS.zip"
    NO_REMOVE_ONE_LEVEL
)

# --- Headers --------------------------------------------------------------
file(INSTALL
    "${ASSETS}/dxcapi.h"
    "${ASSETS}/dxcerrors.h"
    "${ASSETS}/dxcisense.h"
    "${ASSETS}/WinAdapter.h"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}")

# --- Runtime libraries ----------------------------------------------------
file(INSTALL
    "${SOURCE_PATH}/MacOS/lib/libdxcompiler.dylib"
    "${SOURCE_PATH}/MacOS/lib/libdxil.dylib"
    DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
if(NOT DEFINED VCPKG_BUILD_TYPE)
    file(INSTALL
        "${SOURCE_PATH}/MacOS/lib/libdxcompiler.dylib"
        "${SOURCE_PATH}/MacOS/lib/libdxil.dylib"
        DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
endif()

# --- dxc command-line tool ------------------------------------------------
# dxc-3.7 is the real binary (bin/dxc is a relative symlink to it) and its
# LC_RPATH is @executable_path/../lib, so install it under tools/<port>/bin
# with the dylibs in tools/<port>/lib next to it.
file(INSTALL
    "${SOURCE_PATH}/MacOS/bin/dxc-3.7"
    DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin"
    FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
file(RENAME
    "${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin/dxc-3.7"
    "${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin/dxc")
file(INSTALL
    "${SOURCE_PATH}/MacOS/lib/libdxcompiler.dylib"
    "${SOURCE_PATH}/MacOS/lib/libdxil.dylib"
    DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}/lib")

# --- CMake config ---------------------------------------------------------
set(dll_name_dxc  "libdxcompiler.dylib")
set(dll_name_dxil "libdxil.dylib")
set(dll_dir       "lib")
if(NOT DEFINED VCPKG_BUILD_TYPE)
    set(dll_debug_dir "debug/lib")
else()
    set(dll_debug_dir "lib")
endif()
set(lib_name      "@rpath/libdxcompiler.dylib")
set(tool_path     "tools/${PORT}/bin/dxc")

configure_file("${CMAKE_CURRENT_LIST_DIR}/directx-dxc-config.cmake.in"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}-config.cmake"
    @ONLY)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${ASSETS}/LICENSE.txt")
