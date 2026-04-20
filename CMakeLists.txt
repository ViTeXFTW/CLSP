cmake_minimum_required(VERSION 3.30)

project(clsp LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json CONFIG REQUIRED)

add_library(${PROJECT_NAME}_lib STATIC
    src/interface/ILanguageServer.cpp
)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/libraries/
)

target_link_libraries(${PROJECT_NAME}_lib PUBLIC
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(${PROJECT_NAME}_lib PRIVATE
        $<$<CONFIG:Debug>:/W4 /WX>
        $<$<CONFIG:Release>:/O2 /MP>
    )
else()
    target_compile_options(${PROJECT_NAME}_lib PRIVATE
        $<$<CONFIG:Debug>:-Wall -Wextra -Werror>
        $<$<CONFIG:Release>:-O2>
    )
endif()
