vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nihilai-collective/benchmarksuite
    REF "v${VERSION}"
    SHA512 caf0fb107885a721505cb2a6fe7c71f688ee7959b0ffe6b73a6fdddc5d9b0aad40c97b26f0008f530e1fcbe69b9cb1ae61dfd76734574dc90f14eb20be74ca68
    HEAD_REF main
)

set(VCPKG_BUILD_TYPE release)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/License.md")
