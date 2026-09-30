# Engine module targets.
#
# Each engine module is an OBJECT library (FriggaBase, FriggaECS, ...), so it
# owns its source list and per-module compile settings, while `Frigga` still
# archives every object into the single libfrigga.a that the SDK contract
# (FriggaSdk.cmake, PackFriggaSdk.cmake, GenerateModuleExports.cmake) expects.
#
# Modules are NOT linked to each other: the engine headers form one include
# cycle (ECS <-> Scene <-> Asset <-> Animation <-> Audio), so a static-library
# dependency graph between them would not be acyclic. All modules share the
# FriggaDeps interface target instead.

# frigga_add_module(<Name> [DIRS <dir>...] [FILES <file>...])
#   DIRS  directories under src/Frigga, globbed recursively (and the matching
#         include/Frigga/<dir> headers, for IDE visibility)
#   FILES sources relative to src/Frigga
# Every module is appended to the FRIGGA_MODULE_TARGETS list in the caller scope.
function(frigga_add_module NAME)
    cmake_parse_arguments(MOD "" "" "DIRS;FILES" ${ARGN})

    set(_sources)
    foreach (_dir IN LISTS MOD_DIRS)
        file(GLOB_RECURSE _found CONFIGURE_DEPENDS
                "${CMAKE_SOURCE_DIR}/src/Frigga/${_dir}/*.cpp"
                "${CMAKE_SOURCE_DIR}/include/Frigga/${_dir}/*.hpp"
                "${CMAKE_SOURCE_DIR}/include/Frigga/${_dir}/*.h")
        list(APPEND _sources ${_found})
    endforeach ()
    foreach (_file IN LISTS MOD_FILES)
        list(APPEND _sources "${CMAKE_SOURCE_DIR}/src/Frigga/${_file}")
    endforeach ()

    add_library(${NAME} OBJECT ${_sources})

    string(REGEX REPLACE "^Frigga" "" _short "${NAME}")
    add_library(Frigga::${_short} ALIAS ${NAME})

    target_link_libraries(${NAME} PRIVATE Frigga::Deps)
    target_include_directories(${NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include/Frigga)

    # One std-only PCH, built by the first module and shared by the rest.
    if (NOT FRIGGA_PCH_OWNER)
        target_precompile_headers(${NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/Frigga/pch.hpp")
        set(FRIGGA_PCH_OWNER ${NAME} PARENT_SCOPE)
    else ()
        target_precompile_headers(${NAME} REUSE_FROM ${FRIGGA_PCH_OWNER})
    endif ()

    list(APPEND FRIGGA_MODULE_TARGETS ${NAME})
    set(FRIGGA_MODULE_TARGETS ${FRIGGA_MODULE_TARGETS} PARENT_SCOPE)
endfunction()
