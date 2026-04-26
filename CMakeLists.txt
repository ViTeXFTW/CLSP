cmake_minimum_required(VERSION 3.30)

project(CLSP
    VERSION 1.0.0
    DESCRIPTION "C++ LSP Framework"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(clsp STATIC
    src/DocumentStore.cpp
    src/ILanguageServer.cpp
    src/JsonRpc.cpp
    src/StdioTransport.cpp
    src/ThreadPool.cpp
    src/Uri.cpp
)

add_library(CLSP::clsp ALIAS clsp)

target_include_directories(clsp
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(clsp
    PUBLIC nlohmann_json::nlohmann_json
)

target_compile_options(clsp PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:
        $<$<CONFIG:Debug>:/W4 /WX>
        $<$<CONFIG:Release>:/O2 /MP>
    >
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
        $<$<CONFIG:Debug>:-Wall -Wextra -Werror>
        $<$<CONFIG:Release>:-O2>
    >
)

option(CLSP_BUILD_TESTS "Build unit tests" OFF)
if(CLSP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CLSP_INSTALL_DEFAULT ON)
else()
    set(CLSP_INSTALL_DEFAULT OFF)
endif()
option(CLSP_INSTALL "Install CLSP targets, headers, and package files" ${CLSP_INSTALL_DEFAULT})

if(CLSP_INSTALL)
    install(TARGETS clsp
        EXPORT CLSPTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(DIRECTORY include/clsp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT CLSPTargets
        FILE CLSPTargets.cmake
        NAMESPACE CLSP::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLSP
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/CLSPConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/CLSPConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CLSPConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLSP
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CLSPConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CLSPConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLSP
    )

    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/clsp.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/clsp.pc"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/clsp.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()
