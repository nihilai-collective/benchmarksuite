vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nihilai-collective/benchmarksuite
    REF "v${VERSION}"
    SHA512 959852fbfb83cde246b387d0d2b598f5cf1a9bf7bc1c925049826591e0bbc3bc3f99d53bd6abe2ea0e2decc72be4fcf2dc638a60923811b922ad3f1da8d5961a
    HEAD_REF main
)

set(VCPKG_BUILD_TYPE release)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/License.md")
