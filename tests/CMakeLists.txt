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
