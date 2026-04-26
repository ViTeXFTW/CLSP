cmake_minimum_required(VERSION 3.14)
project(clsp_fc_consumer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(clsp SOURCE_DIR "${CLSP_SOURCE_DIR}")
FetchContent_MakeAvailable(clsp)

add_executable(clsp_fc_smoke main.cpp)
target_link_libraries(clsp_fc_smoke PRIVATE CLSP::clsp)
