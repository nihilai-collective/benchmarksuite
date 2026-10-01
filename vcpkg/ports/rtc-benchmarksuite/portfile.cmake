vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nihilai-collective/benchmarksuite
    REF "v${VERSION}"
    SHA512 03873a1f74ceaf968bf755c9f1929ba7f137becc094fa590861a7f4525555a325a2458b8668dfd0f279c8dc3ac34c9b9c544b1e536043ad66468a0c6c6689cf8
    HEAD_REF main
)

set(VCPKG_BUILD_TYPE release)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/License.md")
