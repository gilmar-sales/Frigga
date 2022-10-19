file(GLOB_RECURSE FRIGGA_SRC *.cpp)

find_package(OpenGL REQUIRED)

add_library(frigga ${FRIGGA_SRC})

target_precompile_headers(frigga PUBLIC "pch.hpp")

target_include_directories(frigga INTERFACE
	PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${CMAKE_CURRENT_SOURCE_DIR})
