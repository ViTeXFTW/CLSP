cmake_minimum_required(VERSION 3.14)
project(clsp_fp_consumer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CLSP CONFIG REQUIRED)

add_executable(clsp_fp_smoke main.cpp)
target_link_libraries(clsp_fp_smoke PRIVATE CLSP::clsp)
