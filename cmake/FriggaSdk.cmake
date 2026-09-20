# Shared CMake API for Frigga gameplay modules.
#
# The packaged SDK provides all include files under ${FRIGGA_SDK}/include.
# For engine-source development, FRIGGA_SDK may point at the repository root
# and FRIGGA_BUILD points at the configured Editor build tree.

if(NOT DEFINED FRIGGA_SDK OR FRIGGA_SDK STREQUAL "")
    message(FATAL_ERROR "FRIGGA_SDK must point to a Frigga SDK or source tree")
endif()

set(_FRIGGA_SDK_INCLUDE "${FRIGGA_SDK}/include")
if(NOT EXISTS "${_FRIGGA_SDK_INCLUDE}/Frigga/Module/frigga_module.h")
    message(FATAL_ERROR "Invalid Frigga SDK: ${FRIGGA_SDK}/include/Frigga/Module/frigga_module.h not found")
endif()
if(EXISTS "${FRIGGA_SDK}/FriggaSdkConfig.cmake")
    include("${FRIGGA_SDK}/FriggaSdkConfig.cmake")
endif()
# OFF: modules import Editor.exe (Editor hot-reload). ON: modules import the
# game exe (publish / standalone MyGame.exe).
option(FRIGGA_MODULES_LINK_GAME
       "Link gameplay modules to the game executable instead of the Editor"
       OFF)
set(FRIGGA_SDK_EXPECTED_ABI_VERSION "1" CACHE STRING
    "Expected Frigga gameplay SDK ABI version")
if(DEFINED FRIGGA_SDK_ABI_VERSION AND
   NOT FRIGGA_SDK_ABI_VERSION STREQUAL FRIGGA_SDK_EXPECTED_ABI_VERSION)
    message(FATAL_ERROR
            "Incompatible Frigga SDK ABI: expected ${FRIGGA_SDK_EXPECTED_ABI_VERSION}, "
            "got ${FRIGGA_SDK_ABI_VERSION}")
endif()
if(DEFINED FRIGGA_SDK_CXX_STANDARD AND FRIGGA_SDK_CXX_STANDARD LESS 26)
    message(FATAL_ERROR
            "Frigga SDK requires C++26, but the SDK was built with C++${FRIGGA_SDK_CXX_STANDARD}")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND
   DEFINED FRIGGA_SDK_BUILD_TYPE AND
   NOT FRIGGA_SDK_BUILD_TYPE STREQUAL "Release")
    message(WARNING
            "The game target is Release, but the Frigga SDK was built as "
            "${FRIGGA_SDK_BUILD_TYPE}; project and module code will still use Release flags.")
endif()

set(_FRIGGA_DEPS_INCLUDE "${_FRIGGA_SDK_INCLUDE}")
if(DEFINED FRIGGA_BUILD AND NOT FRIGGA_BUILD STREQUAL "" AND
   EXISTS "${FRIGGA_BUILD}/_deps/freyr-src/include/Freyr")
    set(_FRIGGA_DEPS_INCLUDE "${FRIGGA_BUILD}/_deps/freyr-src/include")
    set(_FRIGGA_SKIRNIR_INCLUDE "${FRIGGA_BUILD}/_deps/skirnir-src/include")
    set(_FRIGGA_FREYA_INCLUDE "${FRIGGA_BUILD}/_deps/freya-src/include")
    set(_FRIGGA_GLM_INCLUDE "${FRIGGA_BUILD}/_deps/glm-src")
    set(_FRIGGA_SIMDJSON_INCLUDE "${FRIGGA_BUILD}/_deps/simdjson-src/include")
else()
    set(_FRIGGA_SKIRNIR_INCLUDE "${_FRIGGA_DEPS_INCLUDE}")
    set(_FRIGGA_FREYA_INCLUDE "${_FRIGGA_DEPS_INCLUDE}")
    set(_FRIGGA_GLM_INCLUDE "${_FRIGGA_DEPS_INCLUDE}")
    set(_FRIGGA_SIMDJSON_INCLUDE "${_FRIGGA_DEPS_INCLUDE}")
endif()

find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)
include(CMakeParseArguments)

set(_FRIGGA_LIB_DIR "${FRIGGA_SDK}/lib")

# Resolve a packaged SDK archive. NAME is the on-disk stem (casing matters for
# Freya/Jolt/SDL3). One Sdk = one build type → unprefixed names only.
function(_frigga_resolve_sdk_library OUT_VAR NAME)
    string(TOLOWER "${NAME}" _lower)
    if(WIN32)
        set(_candidates
                "${_FRIGGA_LIB_DIR}/${NAME}.lib"
                "${_FRIGGA_LIB_DIR}/lib${_lower}.lib"
                "${_FRIGGA_LIB_DIR}/lib${_lower}.a")
    else()
        set(_candidates "${_FRIGGA_LIB_DIR}/lib${NAME}.a")
    endif()
    set(_found "")
    foreach(_candidate IN LISTS _candidates)
        if(EXISTS "${_candidate}")
            set(_found "${_candidate}")
            break()
        endif()
    endforeach()
    set(${OUT_VAR} "${_found}" PARENT_SCOPE)
endfunction()

_frigga_resolve_sdk_library(_FRIGGA_ENGINE_FILE frigga)
if(_FRIGGA_ENGINE_FILE)
    add_library(Frigga::frigga STATIC IMPORTED GLOBAL)
    set(_FRIGGA_INTERFACE_INCLUDES
            "${_FRIGGA_SDK_INCLUDE}"
            "${_FRIGGA_DEPS_INCLUDE}"
            "${_FRIGGA_SKIRNIR_INCLUDE}"
            "${_FRIGGA_FREYA_INCLUDE}"
            "${_FRIGGA_GLM_INCLUDE}"
            "${_FRIGGA_SIMDJSON_INCLUDE}")
    list(REMOVE_DUPLICATES _FRIGGA_INTERFACE_INCLUDES)
    set_target_properties(Frigga::frigga PROPERTIES
            IMPORTED_LOCATION "${_FRIGGA_ENGINE_FILE}"
            INTERFACE_INCLUDE_DIRECTORIES "${_FRIGGA_INTERFACE_INCLUDES}"
            INTERFACE_COMPILE_FEATURES cxx_std_26)
    # Stems match packaged filenames; target aliases are always lowercase.
    # meshoptimizer/assimp are Freya transitive deps required when linking games.
    foreach(_dependency IN ITEMS Freya meshoptimizer glm assimp freyr imgui skirnir simdjson Jolt SDL3 perfetto)
        string(TOLOWER "${_dependency}" _dependency_lower)
        _frigga_resolve_sdk_library(_dependency_file "${_dependency}")
        if(_dependency_file)
            add_library("Frigga::${_dependency_lower}" STATIC IMPORTED GLOBAL)
            set_target_properties("Frigga::${_dependency_lower}" PROPERTIES
                    IMPORTED_LOCATION "${_dependency_file}")
        endif()
    endforeach()
    # Static SDL3 does not embed Windows system deps; mirror SDL3::SDL3-static.
    if(TARGET Frigga::sdl3 AND WIN32)
        set_property(TARGET Frigga::sdl3 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                m kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid
                advapi32 setupapi shell32 dinput8 cfgmgr32)
    endif()
    set(_FRIGGA_ENGINE_LIBS)
    foreach(_dependency IN ITEMS freya meshoptimizer glm assimp freyr perfetto imgui skirnir simdjson jolt sdl3)
        if(TARGET "Frigga::${_dependency}")
            list(APPEND _FRIGGA_ENGINE_LIBS "Frigga::${_dependency}")
        endif()
    endforeach()
    set(_FRIGGA_INTERFACE_DEFINITIONS)
    if(FRIGGA_SDK_FREYR_PROFILING)
        list(APPEND _FRIGGA_INTERFACE_DEFINITIONS FREYR_PROFILING=1)
    endif()
    if(_FRIGGA_INTERFACE_DEFINITIONS)
        set_property(TARGET Frigga::frigga APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS ${_FRIGGA_INTERFACE_DEFINITIONS})
    endif()
    set(_FRIGGA_EXTRA_LIBS)
    if(TARGET Frigga::perfetto AND WIN32)
        list(APPEND _FRIGGA_EXTRA_LIBS ws2_32)
    endif()
    # Assimp is built with ASSIMP_BUILD_ZLIB; pack zlibstatic into Sdk/lib.
    # Prefer that over a system ZLIB so MinGW game configures need no extra deps.
    find_package(ZLIB QUIET)
    if(NOT TARGET ZLIB::ZLIB)
        foreach(_zlib_stem IN ITEMS zlibstatic zlib z)
            _frigga_resolve_sdk_library(_FRIGGA_ZLIB_FILE "${_zlib_stem}")
            if(_FRIGGA_ZLIB_FILE)
                add_library(ZLIB::ZLIB STATIC IMPORTED GLOBAL)
                set_target_properties(ZLIB::ZLIB PROPERTIES
                        IMPORTED_LOCATION "${_FRIGGA_ZLIB_FILE}")
                break()
            endif()
        endforeach()
    endif()
    if(NOT TARGET ZLIB::ZLIB)
        find_package(ZLIB REQUIRED)
    endif()
    target_link_libraries(Frigga::frigga INTERFACE
            ${_FRIGGA_ENGINE_LIBS}
            ${_FRIGGA_EXTRA_LIBS}
            ZLIB::ZLIB
            Vulkan::Vulkan
            ${CMAKE_DL_LIBS}
            Threads::Threads
            $<$<CXX_COMPILER_ID:GNU>:stdc++exp>)
endif()

function(frigga_add_game TARGET)
    set(options)
    set(oneValueArgs NAME DISPLAY_NAME PUBLISHER COPYRIGHT VERSION IDENTIFIER
                     ICON_WINDOWS ICON_LINUX ICON_MACOS)
    cmake_parse_arguments(FRIGGA_GAME "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT TARGET Frigga::frigga)
        message(FATAL_ERROR "Frigga engine libraries were not found in ${FRIGGA_SDK}/lib")
    endif()
    if(NOT FRIGGA_GAME_NAME)
        set(FRIGGA_GAME_NAME "${TARGET}")
    endif()
    if(NOT FRIGGA_GAME_DISPLAY_NAME)
        set(FRIGGA_GAME_DISPLAY_NAME "${FRIGGA_GAME_NAME}")
    endif()
    if(NOT FRIGGA_GAME_VERSION)
        set(FRIGGA_GAME_VERSION "1.0.0")
    endif()
    if(NOT FRIGGA_GAME_PUBLISHER)
        set(FRIGGA_GAME_PUBLISHER "Frigga")
    endif()
    if(NOT FRIGGA_GAME_COPYRIGHT)
        set(FRIGGA_GAME_COPYRIGHT "Copyright (C) ${FRIGGA_GAME_PUBLISHER}")
    endif()
    if(NOT FRIGGA_GAME_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR
                "Frigga game version must contain numeric components only: ${FRIGGA_GAME_VERSION}")
    endif()
    if(NOT FRIGGA_GAME_IDENTIFIER)
        set(FRIGGA_GAME_IDENTIFIER "com.frigga.${TARGET}")
    endif()
    foreach(_icon IN ITEMS ICON_WINDOWS ICON_LINUX ICON_MACOS)
        if(FRIGGA_GAME_${_icon} AND NOT EXISTS "${FRIGGA_GAME_${_icon}}")
            message(FATAL_ERROR "Frigga ${_icon} was not found: ${FRIGGA_GAME_${_icon}}")
        endif()
    endforeach()

    file(GLOB _FRIGGA_RUNTIME_SOURCES CONFIGURE_DEPENDS
            "${FRIGGA_SDK}/Runtime/*.cpp"
            "${FRIGGA_SDK}/Runtime/*.hpp")
    if(NOT _FRIGGA_RUNTIME_SOURCES)
        message(FATAL_ERROR "Frigga Runtime sources were not found in ${FRIGGA_SDK}/Runtime")
    endif()

    add_executable(${TARGET} ${_FRIGGA_RUNTIME_SOURCES})
    target_compile_features(${TARGET} PRIVATE cxx_std_26)
    target_compile_definitions(${TARGET} PRIVATE FRI_PROJECT_RUNTIME)
    # MinGW GCC 16+: avoid strong stdexcept copy ctors vs libstdc++.a (PR 125151).
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_definitions(${TARGET} PRIVATE __cpp_lib_constexpr_exceptions=0)
    endif()
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /std:c++latest /experimental:reflection)
        target_compile_options(${TARGET} PRIVATE "/FI${FRIGGA_SDK}/include/Frigga/Macro.hpp")
    else()
        target_compile_options(${TARGET} PRIVATE
                -std=gnu++26 -freflection
                -include "${FRIGGA_SDK}/include/Frigga/Macro.hpp")
    endif()
    target_include_directories(${TARGET} PRIVATE
            "${FRIGGA_SDK}/Runtime"
            "${_FRIGGA_SDK_INCLUDE}")
    target_link_libraries(${TARGET} PRIVATE Frigga::frigga)
    set_target_properties(${TARGET} PROPERTIES
            ENABLE_EXPORTS TRUE
            OUTPUT_NAME "${FRIGGA_GAME_NAME}")

    if(UNIX AND NOT APPLE)
        target_link_options(${TARGET} PRIVATE -Wl,--export-dynamic)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            # Strip debug only — keep .dynsym so gameplay modules can bind UND
            # symbols (Prefab::Load, etc.) against the published host.
            target_link_options(${TARGET} PRIVATE -Wl,--strip-debug)
        endif()
    elseif(APPLE)
        target_link_options(${TARGET} PRIVATE -Wl,-export_dynamic)
        set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE TRUE)
    endif()

    if(WIN32)
        find_program(_FRIGGA_NM NAMES nm llvm-nm)
        set(_frigga_exports_script "${FRIGGA_SDK}/cmake/GenerateModuleExports.cmake")
        if(_FRIGGA_NM AND EXISTS "${_frigga_exports_script}")
            set(_module_exports "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_module_exports.def")
            set(_lib_frigga "${_FRIGGA_ENGINE_FILE}")
            _frigga_resolve_sdk_library(_lib_freyr freyr)
            _frigga_resolve_sdk_library(_lib_skirnir skirnir)
            _frigga_resolve_sdk_library(_lib_simdjson simdjson)
            # Same Freya screen-UI filter as the Editor host (CMakeLists.txt).
            # Without it, publish modules that call fra::UiContext fail to link.
            _frigga_resolve_sdk_library(_lib_freya Freya)
            set(_freya_module_filter
                    "_ZN3fra9UiContext|_ZNK3fra9UiContext|_ZN3fra8Renderer12GetUiContext|_ZN3fra8Renderer9GetUiDraw|_ZN3fra16RendererAdvanced16GetViewportImage|_ZN3fra11TexturePool23CreateTextureFromMemory|_ZNK3fra6Window8GetWidth|_ZNK3fra6Window9GetHeight")
            # -D vars must appear before -P or the script never sees them.
            set(_exports_cmd
                    ${CMAKE_COMMAND}
                    -D "NM=${_FRIGGA_NM}"
                    -D "LIB0=${_lib_frigga}"
                    -D "LIB1=${_lib_freyr}"
                    -D "LIB2=${_lib_skirnir}"
                    -D "LIB3=${_lib_simdjson}")
            set(_exports_depends
                    "${_frigga_exports_script}"
                    "${_lib_frigga}"
                    "${_lib_freyr}"
                    "${_lib_skirnir}"
                    "${_lib_simdjson}")
            if(_lib_freya)
                list(APPEND _exports_cmd
                        -D "LIB4=${_lib_freya}"
                        -D "FILTER4=${_freya_module_filter}")
                list(APPEND _exports_depends "${_lib_freya}")
            endif()
            list(APPEND _exports_cmd
                    -D "OUT=${_module_exports}"
                    -P "${_frigga_exports_script}")
            add_custom_command(
                    OUTPUT "${_module_exports}"
                    COMMAND ${_exports_cmd}
                    DEPENDS ${_exports_depends}
                    VERBATIM
                    COMMENT "Generate ${TARGET} gameplay module export table")
            target_sources(${TARGET} PRIVATE "${_module_exports}")
        endif()

        string(REPLACE "." "," _version_commas "${FRIGGA_GAME_VERSION}")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_version.rc"
"#include <windows.h>
1 VERSIONINFO
FILEVERSION ${_version_commas},0
PRODUCTVERSION ${_version_commas},0
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x40004L
FILETYPE 0x1L
{
  BLOCK \"StringFileInfo\"
  {
    BLOCK \"040904B0\"
    {
      VALUE \"CompanyName\", \"${FRIGGA_GAME_PUBLISHER}\"
      VALUE \"FileDescription\", \"${FRIGGA_GAME_DISPLAY_NAME}\"
      VALUE \"FileVersion\", \"${FRIGGA_GAME_VERSION}\"
      VALUE \"InternalName\", \"${FRIGGA_GAME_NAME}\"
      VALUE \"LegalCopyright\", \"${FRIGGA_GAME_COPYRIGHT}\"
      VALUE \"OriginalFilename\", \"${FRIGGA_GAME_NAME}.exe\"
      VALUE \"ProductName\", \"${FRIGGA_GAME_DISPLAY_NAME}\"
      VALUE \"ProductVersion\", \"${FRIGGA_GAME_VERSION}\"
    }
  }
  BLOCK \"VarFileInfo\"
  {
    VALUE \"Translation\", 0x0409, 1200
  }
}
")
        if(FRIGGA_GAME_ICON_WINDOWS AND EXISTS "${FRIGGA_GAME_ICON_WINDOWS}")
            file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_version.rc"
                 "IDI_ICON1 ICON DISCARDABLE \"${FRIGGA_GAME_ICON_WINDOWS}\"\n")
        endif()
        target_sources(${TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_version.rc")
    endif()

    if(UNIX AND NOT APPLE)
        set(_desktop "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.desktop")
        file(WRITE "${_desktop}"
"[Desktop Entry]
Type=Application
Name=${FRIGGA_GAME_DISPLAY_NAME}
Comment=${FRIGGA_GAME_DISPLAY_NAME}
Exec=${FRIGGA_GAME_NAME}
Icon=${FRIGGA_GAME_IDENTIFIER}
Categories=Game;
Terminal=false
")
        set(_metainfo "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.metainfo.xml")
        file(WRITE "${_metainfo}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<component type=\"desktop-application\">
  <id>${FRIGGA_GAME_IDENTIFIER}</id>
  <name>${FRIGGA_GAME_DISPLAY_NAME}</name>
  <summary>${FRIGGA_GAME_DISPLAY_NAME}</summary>
  <metadata_license>MIT</metadata_license>
  <provides><binary>${FRIGGA_GAME_NAME}</binary></provides>
  <releases><release version=\"${FRIGGA_GAME_VERSION}\"/></releases>
</component>
")
        install(FILES "${_desktop}" DESTINATION share/applications)
        install(FILES "${_metainfo}" DESTINATION share/metainfo)
        if(FRIGGA_GAME_ICON_LINUX AND EXISTS "${FRIGGA_GAME_ICON_LINUX}")
            install(FILES "${FRIGGA_GAME_ICON_LINUX}"
                    DESTINATION share/icons/hicolor/128x128/apps
                    RENAME "${FRIGGA_GAME_IDENTIFIER}.png")
        endif()
    endif()

    if(APPLE)
        set(_plist "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-Info.plist")
        file(WRITE "${_plist}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\"><dict>
<key>CFBundleDisplayName</key><string>${FRIGGA_GAME_DISPLAY_NAME}</string>
<key>CFBundleExecutable</key><string>${FRIGGA_GAME_NAME}</string>
<key>CFBundleIdentifier</key><string>${FRIGGA_GAME_IDENTIFIER}</string>
<key>CFBundleName</key><string>${FRIGGA_GAME_NAME}</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>${FRIGGA_GAME_VERSION}</string>
<key>CFBundleVersion</key><string>${FRIGGA_GAME_VERSION}</string>
<key>NSHumanReadableCopyright</key><string>${FRIGGA_GAME_COPYRIGHT}</string>
</dict></plist>
")
        set_target_properties(${TARGET} PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST "${_plist}"
                MACOSX_BUNDLE_BUNDLE_NAME "${FRIGGA_GAME_NAME}")
        if(FRIGGA_GAME_ICON_MACOS)
            get_filename_component(_icon_name "${FRIGGA_GAME_ICON_MACOS}" NAME)
            set_target_properties(${TARGET} PROPERTIES
                    MACOSX_BUNDLE_ICON_FILE "${_icon_name}")
            if(EXISTS "${FRIGGA_GAME_ICON_MACOS}")
                set_source_files_properties("${FRIGGA_GAME_ICON_MACOS}"
                        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
                target_sources(${TARGET} PRIVATE "${FRIGGA_GAME_ICON_MACOS}")
            endif()
        endif()
    endif()

    # Modules loaded by the Editor must import Editor.exe. Only published /
    # standalone game builds should bind modules to this executable instead.
    if(FRIGGA_MODULES_LINK_GAME)
        set(FRIGGA_HOST_TARGET "${TARGET}" PARENT_SCOPE)
    endif()
    set(FRIGGA_GAME_TARGET "${TARGET}" PARENT_SCOPE)
endfunction()

function(frigga_add_module TARGET)
    add_library(${TARGET} SHARED ${ARGN})
    target_compile_features(${TARGET} PRIVATE cxx_std_26)
    target_compile_definitions(${TARGET} PRIVATE FRI_MODULE_EXPORTS)
    # Modules do not link Frigga::frigga (Editor import lib only), so Freyr's
    # INTERFACE FREYR_PROFILING never reaches them — propagate explicitly or
    # FREYR_TRACE / WithLabel child scopes compile out as no-ops.
    if(FRIGGA_SDK_FREYR_PROFILING)
        target_compile_definitions(${TARGET} PRIVATE FREYR_PROFILING=1)
    endif()
    # MinGW GCC 16+: avoid strong stdexcept copy ctors vs libstdc++.a (PR 125151).
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_definitions(${TARGET} PRIVATE __cpp_lib_constexpr_exceptions=0)
    endif()

    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /std:c++latest /experimental:reflection)
    else()
        target_compile_options(${TARGET} PRIVATE -std=gnu++26 -freflection)
    endif()

    target_precompile_headers(${TARGET} PRIVATE
            <cstdint>
            <type_traits>
            "${FRIGGA_SDK}/include/Frigga/Module/FriPluginSdk.hpp")

    target_include_directories(${TARGET} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${_FRIGGA_SDK_INCLUDE}"
            "${_FRIGGA_DEPS_INCLUDE}"
            "${_FRIGGA_SKIRNIR_INCLUDE}"
            "${_FRIGGA_FREYA_INCLUDE}"
            "${_FRIGGA_GLM_INCLUDE}"
            "${_FRIGGA_SIMDJSON_INCLUDE}")
    target_link_libraries(${TARGET} PRIVATE Threads::Threads)

    if(UNIX AND NOT APPLE)
        target_link_options(${TARGET} PRIVATE -Wl,--allow-shlib-undefined)
    elseif(WIN32)
        if(DEFINED FRIGGA_HOST_TARGET AND TARGET "${FRIGGA_HOST_TARGET}")
            target_link_libraries(${TARGET} PRIVATE "${FRIGGA_HOST_TARGET}")
        else()
            set(_FRIGGA_EDITOR_IMPLIB "")
            foreach(_candidate IN ITEMS
                    "${FRIGGA_BUILD}/libEditor.dll.a"
                    "${FRIGGA_BUILD}/Editor.lib"
                    "${FRIGGA_BUILD}/libEditor.lib"
                    "${FRIGGA_SDK}/libEditor.dll.a"
                    "${FRIGGA_SDK}/Editor.lib"
                    "${FRIGGA_SDK}/libEditor.lib")
                if(EXISTS "${_candidate}")
                    set(_FRIGGA_EDITOR_IMPLIB "${_candidate}")
                    break()
                endif()
            endforeach()
            if(NOT _FRIGGA_EDITOR_IMPLIB)
                message(FATAL_ERROR "Frigga Editor import library not found")
            endif()
            target_link_libraries(${TARGET} PRIVATE "${_FRIGGA_EDITOR_IMPLIB}")
        endif()
    endif()

    set_target_properties(${TARGET} PROPERTIES
            CXX_STANDARD 26
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS ON
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    if(WIN32)
        set_target_properties(${TARGET} PROPERTIES PREFIX "" IMPORT_PREFIX "")
    endif()
endfunction()

function(frigga_install_game GAME_TARGET GAME_NAME)
    if(TARGET "${GAME_TARGET}")
        install(TARGETS "${GAME_TARGET}"
                RUNTIME DESTINATION .
                BUNDLE DESTINATION .
                LIBRARY DESTINATION .)
        if(EXISTS "${FRIGGA_SDK}/Resources")
            install(DIRECTORY "${FRIGGA_SDK}/Resources/" DESTINATION Resources
                    PATTERN "Modules" EXCLUDE
                    PATTERN "modules" EXCLUDE
                    PATTERN "ProjectTemplate" EXCLUDE
                    PATTERN "Shaders" EXCLUDE
                    PATTERN "OpenSans.ttf" EXCLUDE
                    PATTERN "BootstrapIconsFont.ttf" EXCLUDE)
            if(EXISTS "${FRIGGA_SDK}/Resources/Shaders")
                file(GLOB _frigga_shader_directories
                     LIST_DIRECTORIES true
                     "${FRIGGA_SDK}/Resources/Shaders/*")
                foreach(_frigga_shader_directory IN LISTS _frigga_shader_directories)
                    get_filename_component(_frigga_shader_name
                                           "${_frigga_shader_directory}" NAME)
                    if(IS_DIRECTORY "${_frigga_shader_directory}" AND
                       NOT _frigga_shader_name STREQUAL "Shaders")
                        install(DIRECTORY "${_frigga_shader_directory}/"
                                DESTINATION "Resources/Shaders/${_frigga_shader_name}")
                    endif()
                endforeach()
            endif()
            foreach(_frigga_font IN ITEMS OpenSans.ttf BootstrapIconsFont.ttf)
                if(EXISTS "${FRIGGA_SDK}/Resources/Fonts/${_frigga_font}")
                    install(FILES "${FRIGGA_SDK}/Resources/Fonts/${_frigga_font}"
                            DESTINATION Resources/Fonts)
                endif()
            endforeach()
        endif()
    elseif(EXISTS "${GAME_TARGET}")
        get_filename_component(_frigga_runtime_dir "${GAME_TARGET}" DIRECTORY)
        if(NOT EXISTS "${_frigga_runtime_dir}/Resources")
            message(FATAL_ERROR "Frigga Runtime resources not found beside ${GAME_TARGET}")
        endif()
        install(PROGRAMS "${GAME_TARGET}"
                DESTINATION .
                RENAME "${GAME_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
        install(DIRECTORY "${_frigga_runtime_dir}/Resources/" DESTINATION Resources)
    else()
        message(FATAL_ERROR "Frigga game target or executable not found: ${GAME_TARGET}")
    endif()

    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources/"
            DESTINATION Resources
            PATTERN "Modules" EXCLUDE
            PATTERN "modules" EXCLUDE
            PATTERN "ProjectTemplate" EXCLUDE
            PATTERN "Scenes" EXCLUDE
            PATTERN "*.vert" EXCLUDE
            PATTERN "*.frag" EXCLUDE
            PATTERN "*.comp" EXCLUDE
            PATTERN "*.inc" EXCLUDE)

    foreach(_project_file IN ITEMS frigga.project input.json graphics.json ecs.json)
        if(EXISTS "${CMAKE_SOURCE_DIR}/${_project_file}")
            install(FILES "${CMAKE_SOURCE_DIR}/${_project_file}" DESTINATION .)
        endif()
    endforeach()
    if(EXISTS "${CMAKE_SOURCE_DIR}/Scenes")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/Scenes/"
                DESTINATION Scenes)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/Resources/Scenes")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources/Scenes/"
                DESTINATION Scenes)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/scenes")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/scenes/"
                DESTINATION Scenes)
    endif()

    # Gameplay modules are emitted under the project build/ directory. Install
    # only canonical shared-library artifacts under Modules;
    # hot-reload copies (for example libgameplay.so.reload-*) must never ship.
    install(DIRECTORY "${CMAKE_BINARY_DIR}/"
            DESTINATION Modules
            FILES_MATCHING
            PATTERN "CMakeFiles" EXCLUDE
            PATTERN "_deps" EXCLUDE
            PATTERN "Sdk" EXCLUDE
            PATTERN "Resources" EXCLUDE
            PATTERN "Modules" EXCLUDE
            PATTERN "modules" EXCLUDE
            PATTERN "Testing" EXCLUDE
            REGEX ".*\\.(so|dll|dylib)$")
endfunction()
