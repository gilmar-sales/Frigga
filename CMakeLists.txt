cmake_minimum_required(VERSION 3.29)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CI release builds pass -DFRIGGA_VERSION=<tag without leading v>.
set(FRIGGA_VERSION "0.12.0" CACHE STRING "Frigga package version")
set(FRIGGA_SDK_ABI_VERSION "1" CACHE STRING "Frigga gameplay SDK ABI version")
project(Frigga LANGUAGES CXX VERSION ${FRIGGA_VERSION})

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Frigga requires a 64-bit build; CMAKE_SIZEOF_VOID_P is ${CMAKE_SIZEOF_VOID_P}.")
endif ()

set(CMAKE_CXX_STANDARD 26)

option(FRIGGA_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(FRIGGA_ENABLE_COVERAGE "Enable GCC/Clang coverage instrumentation" OFF)
option(FRIGGA_BUILD_BENCHMARKS "Build Frigga performance benchmarks" OFF)
option(FRIGGA_PLATFORM_CONTRACT_ONLY "Validate platform conditionals without fetching dependencies" OFF)

if(FRIGGA_PLATFORM_CONTRACT_ONLY)
    if(NOT APPLE)
        message(FATAL_ERROR "FRIGGA_PLATFORM_CONTRACT_ONLY is intended for macOS validation")
    endif()
    message(STATUS "Frigga macOS platform contract validated")
    return()
endif()

if(FRIGGA_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
if(FRIGGA_ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

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
        GIT_TAG "v0.37.1"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freyr)

FetchContent_Declare(
        freya
        GIT_REPOSITORY "https://github.com/gilmar-sales/Freya.git"
        GIT_TAG "v0.43.0"
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

FetchContent_Declare(
        miniaudio
        GIT_REPOSITORY "https://github.com/mackron/miniaudio.git"
        GIT_TAG "0.11.22"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(miniaudio)

# Headers needed to scaffold/build gameplay modules from a published Editor.
file(REMOVE_RECURSE
        "${CMAKE_BINARY_DIR}/Resources/modules"
        "${CMAKE_BINARY_DIR}/Resources/Modules"
        "${CMAKE_BINARY_DIR}/Resources/Shaders/Shaders")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackFriggaSdk.cmake")

file(GLOB_RECURSE FRIGGA_SRC CONFIGURE_DEPENDS
        src/Frigga/*.cpp
        include/Frigga/*.hpp
        include/Frigga/*.h)

add_library(frigga ${FRIGGA_SRC})
add_library(frigga::frigga ALIAS frigga)

target_compile_features(frigga PUBLIC cxx_std_26)

if (MSVC)
    target_compile_options(frigga PUBLIC "/ZI")
    target_link_options(frigga PUBLIC "/INCREMENTAL")
endif ()

target_precompile_headers(frigga PUBLIC "src/Frigga/pch.hpp")

target_include_directories(frigga
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Frigga)

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

target_include_directories(frigga PRIVATE ${miniaudio_SOURCE_DIR})
set_source_files_properties(
        src/Frigga/Audio/MiniaudioEngine.cpp
        PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)
if(UNIX AND NOT APPLE)
    target_link_libraries(frigga PRIVATE pthread m)
endif()

set(_FRIGGA_SDK_BINARY_STAMP "${FRIGGA_SDK_DIR}/.engine-libraries.stamp")
set(_FRIGGA_SDK_BINARY_TARGETS
        frigga
        Freya::Freya
        freyr::freyr
        skirnir::skirnir
        simdjson
        imgui::imgui
        Jolt
        SDL3::SDL3)
add_custom_command(
        OUTPUT "${_FRIGGA_SDK_BINARY_STAMP}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FRIGGA_SDK_DIR}/lib"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:frigga>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:frigga>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Freya::Freya>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:Freya::Freya>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:freyr::freyr>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:freyr::freyr>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:skirnir::skirnir>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:skirnir::skirnir>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:simdjson>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:simdjson>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:imgui::imgui>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:imgui::imgui>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Jolt>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:Jolt>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:SDL3::SDL3>"
        COMMAND ${CMAKE_COMMAND} -E touch "${_FRIGGA_SDK_BINARY_STAMP}"
        DEPENDS ${_FRIGGA_SDK_BINARY_TARGETS})
add_custom_target(FriggaSdkLibraries ALL DEPENDS "${_FRIGGA_SDK_BINARY_STAMP}")

set_source_files_properties(
        src/Frigga/Physics/JoltPhysicsWorld.cpp
        PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang>:-fno-reflection>"
)

file(GLOB_RECURSE EDITOR_SRC src/Editor/*.cpp src/Editor/*.hpp)
list(FILTER EDITOR_SRC EXCLUDE REGEX "/Resources/Modules/")

add_executable(Editor ${EDITOR_SRC}
        src/Editor/Main.cpp
        src/Editor/EditorApplication.cpp
        src/Editor/EditorApplication.hpp)

target_compile_features(Editor PUBLIC cxx_std_26)

target_link_libraries(Editor PUBLIC
        frigga::frigga
        imguizmo::imguizmo
)

target_include_directories(Editor PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${freya_SOURCE_DIR}/src"
)

file(GLOB_RECURSE RUNTIME_SRC CONFIGURE_DEPENDS
        src/Runtime/*.cpp
        src/Runtime/*.hpp)

add_executable(Runtime ${RUNTIME_SRC})
target_compile_features(Runtime PUBLIC cxx_std_26)
target_link_libraries(Runtime PUBLIC frigga::frigga)
target_include_directories(Runtime PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime")
add_dependencies(Editor FriggaSdkLibraries)
add_dependencies(Runtime FriggaSdkLibraries)

# Expose Freyr/Frigga/Skirnir/simdjson to gameplay modules.
# Linux: undefined refs in the .so bind to the Editor at dlopen.
# Windows: PE allows ≤65535 exports, so we cannot --export-all-symbols on the
# full Editor. cmake/GenerateModuleExports.cmake dumps the four runtime archives
# (no namespace allowlist) into a .def that feeds ENABLE_EXPORTS' implib.
set_target_properties(Editor PROPERTIES ENABLE_EXPORTS TRUE)
if (UNIX AND NOT APPLE)
    target_link_options(Editor PRIVATE -Wl,--export-dynamic)
    target_link_options(Runtime PRIVATE -Wl,--export-dynamic)
elseif (APPLE)
    target_link_options(Editor PRIVATE -Wl,-export_dynamic)
    target_link_options(Runtime PRIVATE -Wl,-export_dynamic)
endif ()
if (WIN32 AND CMAKE_NM)
    set(_module_exports "${CMAKE_BINARY_DIR}/frigga_module_exports.def")
    add_custom_command(
            OUTPUT "${_module_exports}"
            COMMAND ${CMAKE_COMMAND}
            -D "NM=${CMAKE_NM}"
            -D "LIB0=$<TARGET_FILE:frigga>"
            -D "LIB1=$<TARGET_FILE:freyr>"
            -D "LIB2=$<TARGET_FILE:skirnir>"
            -D "LIB3=$<TARGET_FILE:simdjson>"
            -D "OUT=${_module_exports}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GenerateModuleExports.cmake"
            DEPENDS frigga freyr skirnir simdjson
            "${CMAKE_SOURCE_DIR}/cmake/GenerateModuleExports.cmake"
            VERBATIM
            COMMENT "Generate gameplay module export table")
    target_sources(Editor PRIVATE "${_module_exports}")
    add_custom_command(TARGET Editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_LINKER_FILE:Editor>"
            "${FRIGGA_SDK_DIR}/$<TARGET_LINKER_FILE_NAME:Editor>"
            COMMENT "Pack Editor import lib into Sdk for gameplay modules")
endif ()

# Engine pack (UI fonts, shaders, bundled modules) plus ProjectTemplate for new games.
file(COPY src/Editor/Resources DESTINATION ${CMAKE_BINARY_DIR})
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Resources/Fonts")
file(COPY
        "${CMAKE_SOURCE_DIR}/src/Editor/Resources/OpenSans.ttf"
        "${CMAKE_SOURCE_DIR}/src/Editor/Resources/BootstrapIconsFont.ttf"
        DESTINATION "${CMAKE_BINARY_DIR}/Resources/Fonts")
file(REMOVE
        "${CMAKE_BINARY_DIR}/Resources/OpenSans.ttf"
        "${CMAKE_BINARY_DIR}/Resources/BootstrapIconsFont.ttf")

# --- Tests -----------------------------------------------------------------
enable_testing()
add_subdirectory(test)

if(FRIGGA_BUILD_BENCHMARKS)
    add_executable(frigga_benchmarks
            bench/AssetPipelineBenchmark.cpp)
    target_compile_features(frigga_benchmarks PRIVATE cxx_std_26)
    target_link_libraries(frigga_benchmarks PRIVATE frigga::frigga)
endif()

# --- Install / packaging ---------------------------------------------------
include(GNUInstallDirs)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT Development)

install(TARGETS Editor Runtime
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
