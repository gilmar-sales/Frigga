cmake_minimum_required(VERSION 3.29)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FRIGGA_VERSION "0.12.0" CACHE STRING "Frigga package version")
set(FRIGGA_SDK_ABI_VERSION "1" CACHE STRING "Frigga gameplay SDK ABI version")
project(Frigga LANGUAGES CXX VERSION ${FRIGGA_VERSION})

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    find_program(FREYR_CCACHE_PROGRAM ccache)
    if (FREYR_CCACHE_PROGRAM)
        message(STATUS "Using ccache: ${FREYR_CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${FREYR_CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${FREYR_CCACHE_PROGRAM}")
    else ()
        message(STATUS "ccache not found; compiling without cache")
    endif ()
endif ()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Frigga requires a 64-bit build; CMAKE_SIZEOF_VOID_P is ${CMAKE_SIZEOF_VOID_P}.")
endif ()

set(CMAKE_CXX_STANDARD 26)

# MinGW GCC 16+: C++26 constexpr exceptions emit strong std::logic_error /
# runtime_error copy ctors that collide with libstdc++.a (GCC PR 125151).
if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_definitions(
            $<$<COMPILE_LANGUAGE:CXX>:__cpp_lib_constexpr_exceptions=0>)
endif ()

option(FRIGGA_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(FRIGGA_ENABLE_COVERAGE "Enable GCC/Clang coverage instrumentation" OFF)
option(FRIGGA_BUILD_BENCHMARKS "Build Frigga performance benchmarks" OFF)
option(FRIGGA_PLATFORM_CONTRACT_ONLY "Validate platform conditionals without fetching dependencies" OFF)

if (FRIGGA_PLATFORM_CONTRACT_ONLY)
    if (NOT APPLE)
        message(FATAL_ERROR "FRIGGA_PLATFORM_CONTRACT_ONLY is intended for macOS validation")
    endif ()
    message(STATUS "Frigga macOS platform contract validated")
    return()
endif ()

if (FRIGGA_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif ()
if (FRIGGA_ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif ()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "Target platform" FORCE)
endif ()

include(FetchContent)


set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Frigga SDK uses unprefixed library names" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)

find_package(Vulkan REQUIRED)

set(FRIGGA_SKIRNIR_TAG "v0.24.0")
set(FRIGGA_FREYR_TAG "v0.41.0")
set(FRIGGA_FREYA_TAG "v0.63.0")
set(FRIGGA_SDK_DEPS
        "Skirnir=${FRIGGA_SKIRNIR_TAG};Freyr=${FRIGGA_FREYR_TAG};Freya=${FRIGGA_FREYA_TAG}")

FetchContent_Declare(
        skirnir
        GIT_REPOSITORY "https://github.com/gilmar-sales/Skirnir.git"
        GIT_TAG "${FRIGGA_SKIRNIR_TAG}"
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE TRUE
)
FetchContent_MakeAvailable(skirnir)

FetchContent_Declare(
        freyr
        GIT_REPOSITORY "https://github.com/gilmar-sales/Freyr.git"
        GIT_TAG "${FRIGGA_FREYR_TAG}"
        GIT_SHALLOW TRUE
)
set(FREYR_PROFILING ON CACHE BOOL "Enables Freyr Perfetto profiling" FORCE)
FetchContent_MakeAvailable(freyr)

FetchContent_Declare(
        freya
        GIT_REPOSITORY "https://github.com/gilmar-sales/Freya.git"
        GIT_TAG "${FRIGGA_FREYA_TAG}"
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
# NOTE: ImGuizmo has no version tags upstream; pinned to rolling master on purpose.
# Re-evaluate a commit pin if master ever breaks the Editor build.
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
        GIT_TAG "v5.5.0"
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

# ccache: Assimp already publishes GLOBAL RULE_LAUNCH_COMPILE/LINK when it finds
# ccache, and Freyr sets its own language launchers, so Frigga must not add
# another CMAKE_CXX_COMPILER_LAUNCHER (it double-wraps compiles: ccache ccache).

# Headers needed to scaffold/build gameplay modules from a published Editor.
file(REMOVE_RECURSE
        "${CMAKE_BINARY_DIR}/Resources/modules"
        "${CMAKE_BINARY_DIR}/Resources/Modules"
        "${CMAKE_BINARY_DIR}/Resources/Shaders/Shaders")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackFriggaSdk.cmake")

# Shared external dependencies + build settings for every engine module.
add_library(FriggaDeps INTERFACE)
add_library(Frigga::Deps ALIAS FriggaDeps)

target_compile_features(FriggaDeps INTERFACE cxx_std_26)

if (MSVC)
    # /Zi (not /ZI): Edit-and-Continue PDBs serialize linking and bloat rebuilds.
    target_compile_options(FriggaDeps INTERFACE "/Zi")
    target_link_options(FriggaDeps INTERFACE "/INCREMENTAL")
endif ()

target_include_directories(FriggaDeps INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${Vulkan_INCLUDE_DIRS})

target_link_libraries(FriggaDeps INTERFACE
        Freya::Freya
        freyr::freyr
        imgui::imgui
        skirnir::skirnir
        Jolt
        Vulkan::Vulkan
        SDL3::SDL3
        ${CMAKE_DL_LIBS}
)
if (WIN32)
    target_link_libraries(FriggaDeps INTERFACE dbghelp)
endif ()
target_compile_definitions(FriggaDeps INTERFACE FRIGGA_VERSION_STRING="${FRIGGA_VERSION}")

# Engine modules (see cmake/FriggaModules.cmake). The first one owns the
# std-only, PRIVATE PCH: consumers (Editor/tests) must not inherit it,
# and non-GUI engine code must not pay for ImGui.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FriggaModules.cmake")
frigga_add_module(FriggaBase DIRS Serialization Diagnostics Graphics Rendering)
frigga_add_module(FriggaAsset DIRS Asset Animation)
frigga_add_module(FriggaScene DIRS Scene)
frigga_add_module(FriggaECS DIRS ECS)
frigga_add_module(FriggaPhysics DIRS Physics)
frigga_add_module(FriggaAudio DIRS Audio)
frigga_add_module(FriggaInput DIRS Input)
frigga_add_module(FriggaModule DIRS Module)
frigga_add_module(FriggaGui DIRS Gui Editor)
frigga_add_module(FriggaCore DIRS Core FILES Frigga.cpp)

# libfrigga.a: every module's objects in one archive (SDK contract).
set(_frigga_objects)
foreach (_module IN LISTS FRIGGA_MODULE_TARGETS)
    list(APPEND _frigga_objects $<TARGET_OBJECTS:${_module}>)
endforeach ()
add_library(Frigga STATIC ${_frigga_objects})
add_library(Frigga::Frigga ALIAS Frigga)
# Legacy name, kept for in-tree consumers and existing docs.
add_library(frigga::frigga ALIAS Frigga)
set_target_properties(Frigga PROPERTIES OUTPUT_NAME frigga)

# Same contract as FriggaSdk.cmake: public headers rely on Macro.hpp being
# force-included (FRIGGA_NAMESPACE, BIT), so in-tree consumers get it too.
target_compile_options(Frigga INTERFACE
        "$<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_CURRENT_SOURCE_DIR}/include/Frigga/Macro.hpp>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/Frigga/Macro.hpp>")
target_link_libraries(Frigga PUBLIC Frigga::Deps)

target_include_directories(FriggaAudio PRIVATE ${miniaudio_SOURCE_DIR})
set_source_files_properties(
        src/Frigga/Audio/MiniaudioEngine.cpp
        PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)
if (UNIX AND NOT APPLE)
    target_link_libraries(Frigga PRIVATE pthread m)
endif ()

set(_FRIGGA_SDK_BINARY_STAMP "${FRIGGA_SDK_DIR}/.engine-libraries.stamp")
set(_FRIGGA_SDK_BINARY_TARGETS
        Frigga
        Freya::Freya
        freyr::freyr
        skirnir::skirnir
        simdjson
        imgui::imgui
        Jolt
        SDL3::SDL3)
# Freya transitive deps (and optional profiling) — pack when available.
# zlibstatic comes from Assimp's contrib zlib (ASSIMP_BUILD_ZLIB).
foreach (_optional IN ITEMS assimp meshoptimizer perfetto zlibstatic)
    if (TARGET ${_optional})
        list(APPEND _FRIGGA_SDK_BINARY_TARGETS ${_optional})
    endif ()
endforeach ()
# copy_if_different keeps the SDK stamp cheap: unchanged archives are not
# re-copied, so FriggaSdkLibraries stays a no-op ALL target on Debug rebuilds.
set(_FRIGGA_SDK_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FRIGGA_SDK_DIR}/lib")
foreach (_target IN LISTS _FRIGGA_SDK_BINARY_TARGETS)
    list(APPEND _FRIGGA_SDK_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${_target}>"
            "${FRIGGA_SDK_DIR}/lib/$<TARGET_FILE_NAME:${_target}>")
endforeach ()
add_custom_command(
        OUTPUT "${_FRIGGA_SDK_BINARY_STAMP}"
        ${_FRIGGA_SDK_COPY_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E touch "${_FRIGGA_SDK_BINARY_STAMP}"
        DEPENDS ${_FRIGGA_SDK_BINARY_TARGETS})
add_custom_target(FriggaSdkLibraries ALL DEPENDS "${_FRIGGA_SDK_BINARY_STAMP}")

set_source_files_properties(
        src/Frigga/Physics/JoltPhysicsWorld.cpp
        PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang>:-fno-reflection>"
)

# NOTE: no CONFIGURE_DEPENDS here used to silently drop newly added Editor
# files from the build. Keep it so `cmake --build` picks up new panels after
# a configure; Ninja re-globs automatically on the next build.
file(GLOB_RECURSE EDITOR_SRC CONFIGURE_DEPENDS src/Editor/*.cpp src/Editor/*.hpp)
list(FILTER EDITOR_SRC EXCLUDE REGEX "/Resources/Modules/")

add_executable(Editor ${EDITOR_SRC}
        src/Editor/Main.cpp
        src/Editor/EditorApplication.cpp
        src/Editor/EditorApplication.hpp)
if (WIN32)
    set_target_properties(Editor PROPERTIES WIN32_EXECUTABLE "$<CONFIG:Release>")
endif ()

target_compile_features(Editor PUBLIC cxx_std_26)
target_precompile_headers(Editor PRIVATE "src/Editor/pch.hpp")

target_link_libraries(Editor PUBLIC
        frigga::frigga
        imguizmo::imguizmo
)

target_include_directories(Editor PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${freya_SOURCE_DIR}/src"
)

add_dependencies(Editor FriggaSdkLibraries)

# Headers are seeded at configure time; refresh them on every frigga build so
# new public APIs reach published game hosts without
# requiring a full CMake reconfigure.
add_custom_command(TARGET Frigga POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Frigga"
        "${FRIGGA_SDK_DIR}/include/Frigga"
        COMMENT "Sync Frigga public headers into Sdk")

# Expose Freyr/Frigga/Skirnir/simdjson (+ filtered Freya screen UI) to modules.
# Linux: undefined refs in the .so bind to the host at dlopen (--export-dynamic).
# Prefab/inspector/UI .o files are keep-alived from GameplayModuleHost so static
# GC does not drop APIs modules call (fg::Prefab::Load, fra::UiContext, …).
# Windows: PE allows ≤65535 exports, so we cannot --export-all-symbols on the
# full Editor. cmake/GenerateModuleExports.cmake dumps the runtime archives
# (no namespace allowlist for frigga/freyr/skirnir/simdjson; Freya filtered) into
# a .def that feeds ENABLE_EXPORTS' implib.
set_target_properties(Editor PROPERTIES ENABLE_EXPORTS TRUE)
if (UNIX AND NOT APPLE)
    target_link_options(Editor PRIVATE -Wl,--export-dynamic)
elseif (APPLE)
    target_link_options(Editor PRIVATE -Wl,-export_dynamic)
endif ()
if (WIN32 AND CMAKE_NM)
    set(_module_exports "${CMAKE_BINARY_DIR}/frigga_module_exports.def")
    # Screen UI + façade APIs modules call on the host Renderer / TexturePool.
    set(_freya_module_filter
            "_ZN3fra9UiContext|_ZNK3fra9UiContext|_ZN3fra8Renderer12GetUiContext|_ZN3fra8Renderer9GetUiDraw|_ZN3fra8Renderer16GetBillboardDraw|3fra10SplineRope|_ZN3fra16RendererAdvanced16GetViewportImage|_ZN3fra11TexturePool23CreateTextureFromMemory|_ZNK3fra6Window8GetWidth|_ZNK3fra6Window9GetHeight")
    add_custom_command(
            OUTPUT "${_module_exports}"
            COMMAND ${CMAKE_COMMAND}
            -D "NM=${CMAKE_NM}"
            -D "LIB0=$<TARGET_FILE:Frigga>"
            -D "LIB1=$<TARGET_FILE:freyr>"
            -D "LIB2=$<TARGET_FILE:skirnir>"
            -D "LIB3=$<TARGET_FILE:simdjson>"
            -D "LIB4=$<TARGET_FILE:Freya>"
            -D "FILTER4=${_freya_module_filter}"
            -D "OUT=${_module_exports}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GenerateModuleExports.cmake"
            DEPENDS Frigga freyr skirnir simdjson Freya
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
# NOTE: configure-time file(COPY) is intentional — Editor resolves
# "./Resources" relative to the build dir at launch. After adding resource
# files, re-run `cmake -S . -B build` so they are re-copied.
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

if (FRIGGA_BUILD_BENCHMARKS)
    add_executable(FriggaBenchmarks
            bench/AssetPipelineBenchmark.cpp)
    target_compile_features(FriggaBenchmarks PRIVATE cxx_std_26)
    target_link_libraries(FriggaBenchmarks PRIVATE frigga::frigga)
endif ()

# --- Install / packaging ---------------------------------------------------
include(GNUInstallDirs)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT Development)

install(TARGETS Editor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Engine)

# Shaders and editor assets land in build/Resources; ship them beside the binary
# so relative "./Resources/..." lookups keep working when launched from the
# install prefix / package root.
install(DIRECTORY "${CMAKE_BINARY_DIR}/Resources"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Engine
        OPTIONAL)

install(DIRECTORY "${CMAKE_BINARY_DIR}/Sdk"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Engine
        OPTIONAL)

install(FILES LICENSE.txt README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT Engine)

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
