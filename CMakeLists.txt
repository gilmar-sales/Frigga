cmake_minimum_required(VERSION 3.29)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CI release builds pass -DFRIGGA_VERSION=<tag without leading v>.
set(FRIGGA_VERSION "0.2.0" CACHE STRING "Frigga package version")
project(Frigga LANGUAGES CXX VERSION ${FRIGGA_VERSION})

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
        GIT_TAG "v0.38.0"
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

# Jolt must keep exceptions/RTTI on so it matches Frigga's toolchain.
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics.git"
        GIT_TAG "v5.3.0"
        SOURCE_SUBDIR "Build"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JoltPhysics)

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
        Jolt
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

# --- Tests -----------------------------------------------------------------
add_subdirectory(test)

# --- Install / packaging ---------------------------------------------------
include(GNUInstallDirs)

install(TARGETS Editor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime)

# Shaders and editor assets land in build/Resources; ship them beside the binary
# so relative "./Resources/..." lookups keep working when launched from the
# install prefix / package root.
install(DIRECTORY "${CMAKE_BINARY_DIR}/Resources"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
        OPTIONAL)

install(FILES LICENSE.txt README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT Runtime)

set(CPACK_PACKAGE_NAME "Frigga")
set(CPACK_PACKAGE_VENDOR "Gilmar Sales")
set(CPACK_PACKAGE_CONTACT "gilmar.custodio@outlook.com.br")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Frigga game engine and editor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Frigga")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_FILE_NAME
        "frigga-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)