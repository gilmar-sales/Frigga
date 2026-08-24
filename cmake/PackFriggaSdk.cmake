# Populates ${CMAKE_BINARY_DIR}/Sdk for gameplay plugin scaffolding / CI publish.
# Layout used by gameplay plugin CMake via FRIGGA_SDK (same tree as FRIGGA_ROOT + _deps):
#   Sdk/src/Frigga/...
#   Sdk/_deps/{freyr,skirnir,glm}-src/...

set(FRIGGA_SDK_DIR "${CMAKE_BINARY_DIR}/Sdk")

set(_FRIGGA_SDK_FREYR "${CMAKE_BINARY_DIR}/_deps/freyr-src")
set(_FRIGGA_SDK_SKIRNIR "${CMAKE_BINARY_DIR}/_deps/skirnir-src")
set(_FRIGGA_SDK_GLM "${CMAKE_BINARY_DIR}/_deps/glm-src")
set(_FRIGGA_SDK_SIMDJSON "${CMAKE_BINARY_DIR}/_deps/simdjson-src")
if(DEFINED freyr_SOURCE_DIR)
    set(_FRIGGA_SDK_FREYR "${freyr_SOURCE_DIR}")
endif()
if(DEFINED skirnir_SOURCE_DIR)
    set(_FRIGGA_SDK_SKIRNIR "${skirnir_SOURCE_DIR}")
elseif(DEFINED Skirnir_SOURCE_DIR)
    set(_FRIGGA_SDK_SKIRNIR "${Skirnir_SOURCE_DIR}")
endif()
if(DEFINED glm_SOURCE_DIR)
    set(_FRIGGA_SDK_GLM "${glm_SOURCE_DIR}")
endif()
if(DEFINED simdjson_SOURCE_DIR)
    set(_FRIGGA_SDK_SIMDJSON "${simdjson_SOURCE_DIR}")
endif()

foreach(_dep IN ITEMS "${_FRIGGA_SDK_FREYR}/include/Freyr" "${_FRIGGA_SDK_SKIRNIR}/include/Skirnir"
                      "${_FRIGGA_SDK_GLM}/glm" "${_FRIGGA_SDK_SIMDJSON}/include/simdjson.h")
    if(NOT EXISTS "${_dep}")
        message(FATAL_ERROR "PackFriggaSdk: missing dependency tree at ${_dep}")
    endif()
endforeach()

file(REMOVE_RECURSE "${FRIGGA_SDK_DIR}")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/src/Frigga/Plugin")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/freyr-src/include")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/skirnir-src/include")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/glm-src")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/simdjson-src/include")

file(COPY "${CMAKE_SOURCE_DIR}/src/Frigga/Macro.hpp"
     DESTINATION "${FRIGGA_SDK_DIR}/src/Frigga")
file(COPY "${CMAKE_SOURCE_DIR}/src/Frigga/Plugin"
     DESTINATION "${FRIGGA_SDK_DIR}/src/Frigga"
     FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")
file(COPY "${CMAKE_SOURCE_DIR}/src/Frigga/ECS"
     DESTINATION "${FRIGGA_SDK_DIR}/src/Frigga"
     FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")
file(COPY "${CMAKE_SOURCE_DIR}/src/Frigga/Input"
     DESTINATION "${FRIGGA_SDK_DIR}/src/Frigga"
     FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")

# RigidBody / Character / fg::Physics facade (no Jolt / IPhysicsWorld in the SDK).
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/src/Frigga/Physics")
file(COPY "${CMAKE_SOURCE_DIR}/src/Frigga/Physics/PhysicsBodyHandle.hpp"
          "${CMAKE_SOURCE_DIR}/src/Frigga/Physics/PhysicsCharacterHandle.hpp"
          "${CMAKE_SOURCE_DIR}/src/Frigga/Physics/PhysicsTypes.hpp"
          "${CMAKE_SOURCE_DIR}/src/Frigga/Physics/Physics.hpp"
     DESTINATION "${FRIGGA_SDK_DIR}/src/Frigga/Physics")

# Lightweight Freya event enums for InputMap / plugin includes (no Vulkan).
set(_FRIGGA_SDK_FREYA "${CMAKE_BINARY_DIR}/_deps/freya-src")
if(DEFINED freya_SOURCE_DIR)
    set(_FRIGGA_SDK_FREYA "${freya_SOURCE_DIR}")
elseif(DEFINED Freya_SOURCE_DIR)
    set(_FRIGGA_SDK_FREYA "${Freya_SOURCE_DIR}")
endif()
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/freya-src/include/Freya/Events")
if(NOT EXISTS "${_FRIGGA_SDK_FREYA}/include/Freya/Config.hpp")
    message(FATAL_ERROR "PackFriggaSdk: missing Freya/Config.hpp at ${_FRIGGA_SDK_FREYA}/include/Freya/Config.hpp")
endif()
file(COPY "${_FRIGGA_SDK_FREYA}/include/Freya/Config.hpp"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/freya-src/include/Freya")
file(WRITE "${FRIGGA_SDK_DIR}/_deps/freya-src/include/Freya/Pch.hpp"
     "#pragma once\n#ifndef FREYA_NAMESPACE\n#define FREYA_NAMESPACE fra\n#endif\n#include <cstdint>\n#include <type_traits>\n")
file(COPY "${_FRIGGA_SDK_FREYA}/include/Freya/Events"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/freya-src/include/Freya"
     FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")

file(WRITE "${FRIGGA_SDK_DIR}/src/Frigga/Frigga.hpp"
     "#pragma once\n#include \"Frigga/Macro.hpp\"\n#include \"Frigga/Plugin/FriPluginSdk.hpp\"\n")

file(COPY "${CMAKE_SOURCE_DIR}/src/Editor/Resources/plugins"
     DESTINATION "${FRIGGA_SDK_DIR}")
file(WRITE "${FRIGGA_SDK_DIR}/CMakeLists.txt" "# Frigga gameplay plugin SDK (packaged with Editor)\n")

file(COPY "${_FRIGGA_SDK_FREYR}/include/Freyr"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/freyr-src/include")
file(COPY "${_FRIGGA_SDK_SKIRNIR}/include/Skirnir"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/skirnir-src/include")
file(COPY "${_FRIGGA_SDK_GLM}/glm"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/glm-src")
# Skirnir public headers include <simdjson.h>; plugins compile against the SDK
# without linking simdjson, so the headers must ship with the pack.
file(COPY "${_FRIGGA_SDK_SIMDJSON}/include/simdjson.h"
          "${_FRIGGA_SDK_SIMDJSON}/include/simdjson"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/simdjson-src/include")

# Editor POST_BUILD copies the import lib here; a later configure would
# otherwise wipe it with REMOVE_RECURSE above. Restore from the build tree.
foreach(_implib IN ITEMS libEditor.dll.a Editor.lib libEditor.lib)
    if(EXISTS "${CMAKE_BINARY_DIR}/${_implib}")
        file(COPY "${CMAKE_BINARY_DIR}/${_implib}" DESTINATION "${FRIGGA_SDK_DIR}")
    endif()
endforeach()

message(STATUS "Frigga gameplay SDK: ${FRIGGA_SDK_DIR}")
