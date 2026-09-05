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

    set_target_properties(${TARGET} PROPERTIES
            CXX_STANDARD 26
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS ON
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
    if(WIN32)
        set_target_properties(${TARGET} PROPERTIES PREFIX "" IMPORT_PREFIX "")
    endif()
endfunction()

function(frigga_install_game RUNTIME_EXECUTABLE GAME_NAME)
    if(NOT EXISTS "${RUNTIME_EXECUTABLE}")
        message(FATAL_ERROR "Frigga Runtime executable not found: ${RUNTIME_EXECUTABLE}")
    endif()

    get_filename_component(_frigga_runtime_dir "${RUNTIME_EXECUTABLE}" DIRECTORY)
    if(NOT EXISTS "${_frigga_runtime_dir}/Resources")
        message(FATAL_ERROR "Frigga Runtime resources not found beside ${RUNTIME_EXECUTABLE}")
    endif()

    install(PROGRAMS "${RUNTIME_EXECUTABLE}"
            DESTINATION .
            RENAME "${GAME_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    install(DIRECTORY "${_frigga_runtime_dir}/Resources/"
            DESTINATION Resources)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources/"
            DESTINATION Resources)

    foreach(_project_file IN ITEMS frigga.project input.json ecs.json)
        if(EXISTS "${CMAKE_SOURCE_DIR}/${_project_file}")
            install(FILES "${CMAKE_SOURCE_DIR}/${_project_file}" DESTINATION .)
        endif()
    endforeach()
    if(EXISTS "${CMAKE_SOURCE_DIR}/scenes")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/scenes/" DESTINATION scenes)
    endif()

    # Gameplay modules are emitted under the project build/ directory. Only
    # canonical shared-library artifacts are installed; hot-reload copies
    # (for example libgameplay.so.reload-*) must never ship.
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/build/"
            DESTINATION build
            FILES_MATCHING
            REGEX ".*\\.(so|dll|dylib)$")
endfunction()
