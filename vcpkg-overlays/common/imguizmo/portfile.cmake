vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO CedricGuillemet/ImGuizmo
    REF ba662b119d64f9ab700bb2cd7b2781f9044f5565
    SHA512 682d785b582379914d525985de3a0bc04932b4ed715607127b1803ffba4d9b85165255dca1c18d2fd0934bab43de5d6c9c2d9909ac84d0ddaea12dad1871bcf8
    HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

# ImGuizmo.h only forward-declares ImGui types, so an include-sorted TU that lists it before imgui.h fails to compile.
vcpkg_replace_string("${SOURCE_PATH}/ImGuizmo.h" "#ifdef USE_IMGUI_API
#include \"imconfig.h\"
#endif" "#include <imgui.h>")

# ImGui 1.91 takes ImDrawFlags where ImGuizmo still passes the legacy bool 'closed'.
vcpkg_replace_string("${SOURCE_PATH}/ImGuizmo.cpp" "colors[3 - axis], false, gContext.mStyle.RotationLineThickness)" "colors[3 - axis], ImDrawFlags_None, gContext.mStyle.RotationLineThickness)")
vcpkg_replace_string("${SOURCE_PATH}/ImGuizmo.cpp" "GetColorU32(ROTATION_USING_BORDER), true, gContext.mStyle.RotationLineThickness)" "GetColorU32(ROTATION_USING_BORDER), ImDrawFlags_Closed, gContext.mStyle.RotationLineThickness)")
vcpkg_replace_string("${SOURCE_PATH}/ImGuizmo.cpp" "GetColorU32(DIRECTION_X + i), true, 1.0f)" "GetColorU32(DIRECTION_X + i), ImDrawFlags_Closed, 1.0f)")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS_DEBUG
        -DIMGUIZMO_SKIP_HEADERS=ON
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH share/${PORT})

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
