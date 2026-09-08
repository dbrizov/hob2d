vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO erincatto/box3d
    REF v0.1.0
    SHA512 92eaa76bb0e309d10ae2f408183487e5c55eeff71428ae1a0d1b814d892ac532aca10069414c4b0ef998a754f0bf420a41838c94d70073485f6f9260371aa737
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBOX3D_SAMPLES=OFF
        -DBOX3D_UNIT_TESTS=OFF
        -DBOX3D_BENCHMARKS=OFF
        -DBOX3D_DOCS=OFF
        -DBOX3D_PROFILE=OFF
        -DBOX3D_VALIDATE=OFF
        -DBOX3D_BUILD_SHADERS=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME box3d CONFIG_PATH lib/cmake/box3d)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
