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
find_package(ZLIB REQUIRED)
include(CMakeParseArguments)

set(_FRIGGA_LIB_DIR "${FRIGGA_SDK}/lib")
if(WIN32)
    set(_FRIGGA_ENGINE_FILE "${_FRIGGA_LIB_DIR}/frigga.lib")
    if(NOT EXISTS "${_FRIGGA_ENGINE_FILE}")
        set(_FRIGGA_ENGINE_FILE "${_FRIGGA_LIB_DIR}/libfrigga.lib")
    endif()
else()
    set(_FRIGGA_ENGINE_FILE "${_FRIGGA_LIB_DIR}/libfrigga.a")
endif()
if(EXISTS "${_FRIGGA_ENGINE_FILE}")
    add_library(Frigga::frigga STATIC IMPORTED GLOBAL)
    set_target_properties(Frigga::frigga PROPERTIES
            IMPORTED_LOCATION "${_FRIGGA_ENGINE_FILE}"
            INTERFACE_INCLUDE_DIRECTORIES "${_FRIGGA_SDK_INCLUDE}")
    foreach(_dependency IN ITEMS freya glm assimp freyr imgui skirnir simdjson jolt sdl3)
        string(TOLOWER "${_dependency}" _dependency_lower)
        string(TOUPPER "${_dependency}" _dependency_upper)
        if(WIN32)
            if(_dependency STREQUAL "freya")
                set(_dependency_file "${_FRIGGA_LIB_DIR}/Freya.lib")
            elseif(_dependency STREQUAL "jolt")
                set(_dependency_file "${_FRIGGA_LIB_DIR}/Jolt.lib")
            elseif(_dependency STREQUAL "sdl3")
                set(_dependency_file "${_FRIGGA_LIB_DIR}/SDL3.lib")
            else()
                set(_dependency_file "${_FRIGGA_LIB_DIR}/${_dependency}.lib")
            endif()
        else()
            if(_dependency STREQUAL "freya")
                set(_dependency_file "${_FRIGGA_LIB_DIR}/libFreya.a")
            elseif(_dependency STREQUAL "jolt")
                set(_dependency_file "${_FRIGGA_LIB_DIR}/libJolt.a")
            elseif(_dependency STREQUAL "sdl3")
                set(_dependency_file "${_FRIGGA_LIB_DIR}/libSDL3.a")
            else()
                set(_dependency_file "${_FRIGGA_LIB_DIR}/lib${_dependency_lower}.a")
            endif()
        endif()
        if(EXISTS "${_dependency_file}")
            add_library("Frigga::${_dependency_lower}" STATIC IMPORTED GLOBAL)
            set_target_properties("Frigga::${_dependency_lower}" PROPERTIES
                    IMPORTED_LOCATION "${_dependency_file}")
        endif()
    endforeach()
    set(_FRIGGA_ENGINE_LIBS)
    foreach(_dependency IN ITEMS freya glm assimp freyr imgui skirnir simdjson jolt sdl3)
        if(TARGET "Frigga::${_dependency}")
            list(APPEND _FRIGGA_ENGINE_LIBS "Frigga::${_dependency}")
        endif()
    endforeach()
    target_link_libraries(Frigga::frigga INTERFACE
            ${_FRIGGA_ENGINE_LIBS}
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
            target_link_options(${TARGET} PRIVATE -s)
        endif()
    elseif(APPLE)
        target_link_options(${TARGET} PRIVATE -Wl,-export_dynamic)
        set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE TRUE)
    endif()

    if(WIN32)
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

    set(FRIGGA_HOST_TARGET "${TARGET}" PARENT_SCOPE)
    set(FRIGGA_GAME_TARGET "${TARGET}" PARENT_SCOPE)
endfunction()

function(frigga_add_module TARGET)
    add_library(${TARGET} SHARED ${ARGN})
    target_compile_features(${TARGET} PRIVATE cxx_std_26)
    target_compile_definitions(${TARGET} PRIVATE FRI_MODULE_EXPORTS)

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
            PATTERN "*.vert" EXCLUDE
            PATTERN "*.frag" EXCLUDE
            PATTERN "*.comp" EXCLUDE
            PATTERN "*.inc" EXCLUDE)

    foreach(_project_file IN ITEMS frigga.project input.json ecs.json)
        if(EXISTS "${CMAKE_SOURCE_DIR}/${_project_file}")
            install(FILES "${CMAKE_SOURCE_DIR}/${_project_file}" DESTINATION .)
        endif()
    endforeach()
    if(EXISTS "${CMAKE_SOURCE_DIR}/scenes")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/scenes/" DESTINATION scenes)
    endif()

    # Gameplay modules are emitted under the project build/ directory. Install
    # only canonical shared-library artifacts under Resources/Modules;
    # hot-reload copies (for example libgameplay.so.reload-*) must never ship.
    install(DIRECTORY "${CMAKE_BINARY_DIR}/"
            DESTINATION Resources/Modules
            FILES_MATCHING
            PATTERN "CMakeFiles" EXCLUDE
            PATTERN "_deps" EXCLUDE
            PATTERN "Sdk" EXCLUDE
            PATTERN "Resources" EXCLUDE
            PATTERN "Testing" EXCLUDE
            REGEX ".*\\.(so|dll|dylib)$")
endfunction()
