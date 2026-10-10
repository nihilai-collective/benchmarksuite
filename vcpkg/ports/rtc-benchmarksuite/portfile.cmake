vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nihilai-collective/benchmarksuite
    REF "v${VERSION}"
    SHA512 898556e2747190e34e208d81ed20714f959a7a21619a351b1795ea0427d89a860d77ccb7571bd6262a8c89379640c7e4c61b64ea4cfb90093ea0ef7a70b1244a
    HEAD_REF main
)

set(VCPKG_BUILD_TYPE release)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/License.md")
