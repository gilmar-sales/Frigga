cmake_minimum_required(VERSION 3.29)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Frigga LANGUAGES CXX VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 26)

include(FetchContent)

FetchContent_Declare(
        freyr
        GIT_REPOSITORY "https://github.com/gilmar-sales/Freyr.git"
        GIT_TAG "v0.29.0"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freyr)

FetchContent_Declare(
        freya
        GIT_REPOSITORY "https://github.com/gilmar-sales/Freya.git"
        GIT_TAG "v0.21.0"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freya)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY "https://github.com/gilmar-sales/imgui.git"
        GIT_TAG "docking"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUIZMO_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        imguizmo
        GIT_REPOSITORY "https://github.com/CedricGuillemet/ImGuizmo.git"
        GIT_TAG "master"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imguizmo)
target_link_libraries(imguizmo PUBLIC imgui::imgui)

file(GLOB_RECURSE FRIGGA_SRC src/Frigga/*.cpp src/Frigga/*.hpp)

add_library(frigga ${FRIGGA_SRC}
        src/Frigga/Core/AbstractApplication.cpp)
add_library(frigga::frigga ALIAS frigga)

target_compile_features(frigga PUBLIC cxx_std_26)

if (MSVC)
    target_compile_options(frigga PUBLIC "/ZI")
    target_link_options(frigga PUBLIC "/INCREMENTAL")
endif ()

target_precompile_headers(frigga PUBLIC "src/Frigga/pch.hpp")

target_include_directories(frigga INTERFACE
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        src)

target_include_directories(frigga PUBLIC ${Vulkan_INCLUDE_DIRS})

target_link_libraries(frigga PUBLIC
        Freya::Freya
        freyr::freyr
        imgui::imgui
)

file(GLOB_RECURSE EDITOR_SRC src/Editor/*.cpp src/Editor/*.hpp)

add_executable(Editor ${EDITOR_SRC}
        src/Editor/Main.cpp
        src/Editor/EditorApplication.cpp
        src/Editor/EditorApplication.hpp)

target_compile_features(Editor PUBLIC cxx_std_26)

target_link_libraries(Editor PUBLIC
        frigga::frigga
        imguizmo::imguizmo
)
file(COPY src/Editor/Resources DESTINATION ${CMAKE_BINARY_DIR})
