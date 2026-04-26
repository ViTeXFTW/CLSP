cmake_minimum_required(VERSION 3.14)
project(clsp_asd_consumer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CLSP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLSP_INSTALL     OFF CACHE BOOL "" FORCE)

add_subdirectory("${CLSP_SOURCE_DIR}" clsp_src)

add_executable(clsp_asd_smoke main.cpp)
target_link_libraries(clsp_asd_smoke PRIVATE CLSP::clsp)
