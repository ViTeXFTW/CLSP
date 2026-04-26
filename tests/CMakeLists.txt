find_package(GTest CONFIG REQUIRED)

add_executable(clsp_tests
    JsonRpcTest.cpp
    ProtocolTest.cpp
    LifecycleTest.cpp
    ServerMessagingTest.cpp
    UriTest.cpp
    PositionEncodingTest.cpp
    ThreadPoolTest.cpp
    DocumentStoreTest.cpp
    CancellationTest.cpp
    DispatchTest.cpp
    DiagnosticsTest.cpp
    HoverTest.cpp
    CompletionTest.cpp
)

target_link_libraries(clsp_tests PRIVATE
    clsp
    GTest::gtest_main
)

# White-box access: internal headers for JsonRpc tests
target_include_directories(clsp_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

include(GoogleTest)
gtest_discover_tests(clsp_tests)

# ---------------------------------------------------------------------------
# Packaging / integration tests
# ---------------------------------------------------------------------------
set(_pkg_dir  "${CMAKE_CURRENT_SOURCE_DIR}/packaging")
set(_pkg_bin  "${CMAKE_CURRENT_BINARY_DIR}/packaging")
set(_inst_pfx "${CMAKE_CURRENT_BINARY_DIR}/packaging_install")

set(_fwd
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
    "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
    "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
    "-DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
)

# ── Install fixture (find_package test depends on this) ───────────────────
add_test(NAME packaging.install_clsp
    COMMAND ${CMAKE_COMMAND} --install "${PROJECT_BINARY_DIR}"
        --prefix "${_inst_pfx}" --config "$<CONFIG>")
set_tests_properties(packaging.install_clsp PROPERTIES
    FIXTURES_SETUP clsp_packaging_install)

# ── find_package ──────────────────────────────────────────────────────────
add_test(NAME packaging.find_package.configure
    COMMAND ${CMAKE_COMMAND}
        -S "${_pkg_dir}/find_package" -B "${_pkg_bin}/find_package"
        -G "${CMAKE_GENERATOR}" ${_fwd}
        "-DCMAKE_BUILD_TYPE=$<CONFIG>"
        "-DCMAKE_PREFIX_PATH=${_inst_pfx}")
set_tests_properties(packaging.find_package.configure PROPERTIES
    FIXTURES_REQUIRED clsp_packaging_install
    FIXTURES_SETUP    clsp_fp_configured)

add_test(NAME packaging.find_package.build
    COMMAND ${CMAKE_COMMAND} --build "${_pkg_bin}/find_package" --config "$<CONFIG>")
set_tests_properties(packaging.find_package.build PROPERTIES
    FIXTURES_REQUIRED clsp_fp_configured
    FIXTURES_SETUP    clsp_fp_built)

add_test(NAME packaging.find_package.run
    COMMAND "${_pkg_bin}/find_package/clsp_fp_smoke${CMAKE_EXECUTABLE_SUFFIX}")
set_tests_properties(packaging.find_package.run PROPERTIES
    FIXTURES_REQUIRED clsp_fp_built)

# ── FetchContent ──────────────────────────────────────────────────────────
add_test(NAME packaging.fetch_content.configure
    COMMAND ${CMAKE_COMMAND}
        -S "${_pkg_dir}/fetch_content" -B "${_pkg_bin}/fetch_content"
        -G "${CMAKE_GENERATOR}" ${_fwd}
        "-DCMAKE_BUILD_TYPE=$<CONFIG>"
        "-DCLSP_SOURCE_DIR=${PROJECT_SOURCE_DIR}")
set_tests_properties(packaging.fetch_content.configure PROPERTIES
    FIXTURES_SETUP clsp_fc_configured)

add_test(NAME packaging.fetch_content.build
    COMMAND ${CMAKE_COMMAND} --build "${_pkg_bin}/fetch_content" --config "$<CONFIG>")
set_tests_properties(packaging.fetch_content.build PROPERTIES
    FIXTURES_REQUIRED clsp_fc_configured
    FIXTURES_SETUP    clsp_fc_built)

add_test(NAME packaging.fetch_content.run
    COMMAND "${_pkg_bin}/fetch_content/clsp_fc_smoke${CMAKE_EXECUTABLE_SUFFIX}")
set_tests_properties(packaging.fetch_content.run PROPERTIES
    FIXTURES_REQUIRED clsp_fc_built)

# ── add_subdirectory ──────────────────────────────────────────────────────
add_test(NAME packaging.add_subdirectory.configure
    COMMAND ${CMAKE_COMMAND}
        -S "${_pkg_dir}/add_subdirectory" -B "${_pkg_bin}/add_subdirectory"
        -G "${CMAKE_GENERATOR}" ${_fwd}
        "-DCMAKE_BUILD_TYPE=$<CONFIG>"
        "-DCLSP_SOURCE_DIR=${PROJECT_SOURCE_DIR}")
set_tests_properties(packaging.add_subdirectory.configure PROPERTIES
    FIXTURES_SETUP clsp_asd_configured)

add_test(NAME packaging.add_subdirectory.build
    COMMAND ${CMAKE_COMMAND} --build "${_pkg_bin}/add_subdirectory" --config "$<CONFIG>")
set_tests_properties(packaging.add_subdirectory.build PROPERTIES
    FIXTURES_REQUIRED clsp_asd_configured
    FIXTURES_SETUP    clsp_asd_built)

add_test(NAME packaging.add_subdirectory.run
    COMMAND "${_pkg_bin}/add_subdirectory/clsp_asd_smoke${CMAKE_EXECUTABLE_SUFFIX}")
set_tests_properties(packaging.add_subdirectory.run PROPERTIES
    FIXTURES_REQUIRED clsp_asd_built)
