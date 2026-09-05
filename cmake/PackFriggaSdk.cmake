# Build a self-contained gameplay SDK in ${CMAKE_BINARY_DIR}/Sdk.
#
# SDK layout:
#   Sdk/include/Frigga       Frigga public API
#   Sdk/include/{Freya,Freyr,Skirnir,glm,simdjson} dependency headers
#   Sdk/cmake/FriggaSdk.cmake shared gameplay-module CMake API
#   Sdk/modules              bundled example modules

set(FRIGGA_SDK_DIR "${CMAKE_BINARY_DIR}/Sdk")

set(_FRIGGA_SDK_FREYR "${CMAKE_BINARY_DIR}/_deps/freyr-src")
set(_FRIGGA_SDK_SKIRNIR "${CMAKE_BINARY_DIR}/_deps/skirnir-src")
set(_FRIGGA_SDK_GLM "${CMAKE_BINARY_DIR}/_deps/glm-src")
set(_FRIGGA_SDK_SIMDJSON "${CMAKE_BINARY_DIR}/_deps/simdjson-src")
set(_FRIGGA_SDK_FREYA "${CMAKE_BINARY_DIR}/_deps/freya-src")
foreach(_dep IN ITEMS FREYR SKIRNIR GLM SIMDJSON FREYA)
    string(TOLOWER "${_dep}" _dep_name)
    if(DEFINED ${_dep_name}_SOURCE_DIR)
        set(_FRIGGA_SDK_${_dep} "${${_dep_name}_SOURCE_DIR}")
    endif()
endforeach()
if(DEFINED Skirnir_SOURCE_DIR)
    set(_FRIGGA_SDK_SKIRNIR "${Skirnir_SOURCE_DIR}")
endif()
if(DEFINED Freya_SOURCE_DIR)
    set(_FRIGGA_SDK_FREYA "${Freya_SOURCE_DIR}")
endif()

foreach(_required IN ITEMS
        "${_FRIGGA_SDK_FREYR}/include/Freyr"
        "${_FRIGGA_SDK_SKIRNIR}/include/Skirnir"
        "${_FRIGGA_SDK_GLM}/glm"
        "${_FRIGGA_SDK_SIMDJSON}/include/simdjson.h"
        "${_FRIGGA_SDK_FREYA}/include/Freya/Config.hpp")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "PackFriggaSdk: missing dependency at ${_required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${FRIGGA_SDK_DIR}")
file(MAKE_DIRECTORY
        "${FRIGGA_SDK_DIR}/include"
        "${FRIGGA_SDK_DIR}/cmake")

file(COPY "${CMAKE_SOURCE_DIR}/include/Frigga"
     DESTINATION "${FRIGGA_SDK_DIR}/include")
file(COPY "${_FRIGGA_SDK_FREYR}/include/Freyr"
     DESTINATION "${FRIGGA_SDK_DIR}/include")
file(COPY "${_FRIGGA_SDK_SKIRNIR}/include/Skirnir"
     DESTINATION "${FRIGGA_SDK_DIR}/include")
file(COPY "${_FRIGGA_SDK_GLM}/glm"
     DESTINATION "${FRIGGA_SDK_DIR}/include")
file(COPY "${_FRIGGA_SDK_SIMDJSON}/include/simdjson.h"
          "${_FRIGGA_SDK_SIMDJSON}/include/simdjson"
     DESTINATION "${FRIGGA_SDK_DIR}/include")

file(COPY "${_FRIGGA_SDK_FREYA}/include/Freya/Config.hpp"
     DESTINATION "${FRIGGA_SDK_DIR}/include/Freya")
file(WRITE "${FRIGGA_SDK_DIR}/include/Freya/Pch.hpp"
     "#pragma once\n#ifndef FREYA_NAMESPACE\n#define FREYA_NAMESPACE fra\n#endif\n#include <cstdint>\n#include <type_traits>\n")
file(COPY "${_FRIGGA_SDK_FREYA}/include/Freya/Events"
     DESTINATION "${FRIGGA_SDK_DIR}/include/Freya")

file(COPY "${CMAKE_SOURCE_DIR}/cmake/FriggaSdk.cmake"
     DESTINATION "${FRIGGA_SDK_DIR}/cmake")
file(COPY "${CMAKE_SOURCE_DIR}/src/Editor/Resources/modules"
     DESTINATION "${FRIGGA_SDK_DIR}")
file(WRITE "${FRIGGA_SDK_DIR}/CMakeLists.txt"
     "# Frigga gameplay module SDK (packaged with Editor)\n")

# The Editor POST_BUILD step places a Windows import library in the SDK.
foreach(_implib IN ITEMS libEditor.dll.a Editor.lib libEditor.lib)
    if(EXISTS "${CMAKE_BINARY_DIR}/${_implib}")
        file(COPY "${CMAKE_BINARY_DIR}/${_implib}" DESTINATION "${FRIGGA_SDK_DIR}")
    endif()
endforeach()

message(STATUS "Frigga gameplay SDK: ${FRIGGA_SDK_DIR}")
