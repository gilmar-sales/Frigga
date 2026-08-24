cmake_minimum_required(VERSION 3.29)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CI release builds pass -DFRIGGA_VERSION=<tag without leading v>.
set(FRIGGA_VERSION "0.10.0" CACHE STRING "Frigga package version")
project(Frigga LANGUAGES CXX VERSION ${FRIGGA_VERSION})

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Frigga requires a 64-bit build; CMAKE_SIZEOF_VOID_P is ${CMAKE_SIZEOF_VOID_P}.")
endif ()

set(CMAKE_CXX_STANDARD 26)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "Target platform" FORCE)
endif ()

include(FetchContent)

find_package(Vulkan REQUIRED)

FetchContent_Declare(
        skirnir
        GIT_REPOSITORY "https://github.com/gilmar-sales/Skirnir.git"
        GIT_TAG "v0.23.2"
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE TRUE
)
FetchContent_MakeAvailable(skirnir)

FetchContent_Declare(
        freyr
        GIT_REPOSITORY "https://github.com/gilmar-sales/Freyr.git"
        GIT_TAG "v0.34.0"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freyr)

FetchContent_Declare(
        freya
        GIT_REPOSITORY "https://github.com/gilmar-sales/Freya.git"
        GIT_TAG "v0.42.3"
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

# Headers needed to scaffold/build gameplay plugins from a published Editor.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackFriggaSdk.cmake")

file(GLOB_RECURSE FRIGGA_SRC src/Frigga/*.cpp src/Frigga/*.hpp)

add_library(frigga ${FRIGGA_SRC})
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
        skirnir::skirnir
        Jolt
        Vulkan::Vulkan
        SDL3::SDL3
        ${CMAKE_DL_LIBS}
)

set_source_files_properties(
        src/Frigga/Physics/JoltPhysicsWorld.cpp
        PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang>:-fno-reflection>"
)

file(GLOB_RECURSE EDITOR_SRC src/Editor/*.cpp src/Editor/*.hpp)
list(FILTER EDITOR_SRC EXCLUDE REGEX "/Resources/plugins/")

add_executable(Editor ${EDITOR_SRC}
        src/Editor/Main.cpp
        src/Editor/EditorApplication.cpp
        src/Editor/EditorApplication.hpp)

target_compile_features(Editor PUBLIC cxx_std_26)

target_link_libraries(Editor PUBLIC
        frigga::frigga
        imguizmo::imguizmo
)

# Expose Freyr/Frigga/Skirnir/simdjson to gameplay plugins.
# Linux: undefined refs in the .so bind to the Editor at dlopen.
# Windows: PE allows ≤65535 exports, so we cannot --export-all-symbols on the
# full Editor. cmake/GeneratePluginExports.cmake dumps the four runtime archives
# (no namespace allowlist) into a .def that feeds ENABLE_EXPORTS' implib.
set_target_properties(Editor PROPERTIES ENABLE_EXPORTS TRUE)
if (UNIX AND NOT APPLE)
    target_link_options(Editor PRIVATE -Wl,--export-dynamic)
elseif (APPLE)
    target_link_options(Editor PRIVATE -Wl,-export_dynamic)
endif ()
if (WIN32 AND CMAKE_NM)
    set(_plugin_exports "${CMAKE_BINARY_DIR}/frigga_plugin_exports.def")
    add_custom_command(
            OUTPUT "${_plugin_exports}"
            COMMAND ${CMAKE_COMMAND}
            -D "NM=${CMAKE_NM}"
            -D "LIB0=$<TARGET_FILE:frigga>"
            -D "LIB1=$<TARGET_FILE:freyr>"
            -D "LIB2=$<TARGET_FILE:skirnir>"
            -D "LIB3=$<TARGET_FILE:simdjson>"
            -D "OUT=${_plugin_exports}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GeneratePluginExports.cmake"
            DEPENDS frigga freyr skirnir simdjson
            "${CMAKE_SOURCE_DIR}/cmake/GeneratePluginExports.cmake"
            VERBATIM
            COMMENT "Generate gameplay plugin export table")
    target_sources(Editor PRIVATE "${_plugin_exports}")
    add_custom_command(TARGET Editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_LINKER_FILE:Editor>"
            "${FRIGGA_SDK_DIR}/$<TARGET_LINKER_FILE_NAME:Editor>"
            COMMENT "Pack Editor import lib into Sdk for gameplay plugins")
endif ()

# Engine pack (UI fonts, shaders, bundled plugins) plus ProjectTemplate for new games.
file(COPY src/Editor/Resources DESTINATION ${CMAKE_BINARY_DIR})

# --- Tests -----------------------------------------------------------------
enable_testing()
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

install(DIRECTORY "${CMAKE_BINARY_DIR}/Sdk"
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