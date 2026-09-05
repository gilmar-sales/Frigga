# Build a self-contained gameplay SDK in ${CMAKE_BINARY_DIR}/Sdk.
#
# SDK layout:
#   Sdk/include/Frigga       Frigga public API
#   Sdk/include/{Freya,Freyr,Skirnir,glm,simdjson} dependency headers
#   Sdk/cmake/FriggaSdk.cmake shared gameplay-module CMake API
#   Sdk/Modules              bundled example modules

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

file(COPY "${_FRIGGA_SDK_FREYA}/include/Freya"
     DESTINATION "${FRIGGA_SDK_DIR}/include")
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/sdl-src/include/SDL3")
    file(COPY "${CMAKE_BINARY_DIR}/_deps/sdl-src/include/SDL3"
         DESTINATION "${FRIGGA_SDK_DIR}/include")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/joltphysics-src/Jolt")
    file(COPY "${CMAKE_BINARY_DIR}/_deps/joltphysics-src/Jolt"
         DESTINATION "${FRIGGA_SDK_DIR}/include")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/assimp-src/include/assimp")
    file(COPY "${CMAKE_BINARY_DIR}/_deps/assimp-src/include/assimp"
         DESTINATION "${FRIGGA_SDK_DIR}/include")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/assimp-build/include/assimp")
    file(COPY "${CMAKE_BINARY_DIR}/_deps/assimp-build/include/assimp"
         DESTINATION "${FRIGGA_SDK_DIR}/include")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui.h")
    file(GLOB _FRIGGA_IMGUI_HEADERS
         "${CMAKE_BINARY_DIR}/_deps/imgui-src/*.h")
    file(COPY ${_FRIGGA_IMGUI_HEADERS}
         DESTINATION "${FRIGGA_SDK_DIR}/include")
endif()

file(COPY "${CMAKE_SOURCE_DIR}/cmake/FriggaSdk.cmake"
     DESTINATION "${FRIGGA_SDK_DIR}/cmake")
file(COPY "${CMAKE_SOURCE_DIR}/src/Runtime"
     DESTINATION "${FRIGGA_SDK_DIR}")
if(EXISTS "${CMAKE_BINARY_DIR}/Resources")
    file(COPY "${CMAKE_BINARY_DIR}/Resources"
         DESTINATION "${FRIGGA_SDK_DIR}")
endif()
file(COPY "${CMAKE_SOURCE_DIR}/src/Editor/Resources/Modules"
     DESTINATION "${FRIGGA_SDK_DIR}")
file(WRITE "${FRIGGA_SDK_DIR}/CMakeLists.txt"
     "# Frigga gameplay module SDK (packaged with Editor)\n")
file(WRITE "${FRIGGA_SDK_DIR}/FriggaSdkConfig.cmake"
     "set(FRIGGA_SDK_BUILD_TYPE \"${CMAKE_BUILD_TYPE}\")\n")

# The Editor POST_BUILD step places a Windows import library in the SDK.
foreach(_implib IN ITEMS libEditor.dll.a Editor.lib libEditor.lib)
    if(EXISTS "${CMAKE_BINARY_DIR}/${_implib}")
        file(COPY "${CMAKE_BINARY_DIR}/${_implib}" DESTINATION "${FRIGGA_SDK_DIR}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/lib")
foreach(_library IN ITEMS
        "${CMAKE_BINARY_DIR}/libfrigga.a"
        "${CMAKE_BINARY_DIR}/_deps/freya-build/libFreya.a"
        "${CMAKE_BINARY_DIR}/_deps/glm-build/glm/libglm.a"
        "${CMAKE_BINARY_DIR}/_deps/assimp-build/lib/libassimp.a"
        "${CMAKE_BINARY_DIR}/_deps/freyr-build/libfreyr.a"
        "${CMAKE_BINARY_DIR}/_deps/skirnir-build/libskirnir.a"
        "${CMAKE_BINARY_DIR}/_deps/simdjson-build/libsimdjson.a"
        "${CMAKE_BINARY_DIR}/_deps/imgui-build/libimgui.a"
        "${CMAKE_BINARY_DIR}/_deps/joltphysics-build/libJolt.a"
        "${CMAKE_BINARY_DIR}/_deps/sdl-build/libSDL3.a")
    if(EXISTS "${_library}")
        file(COPY "${_library}" DESTINATION "${FRIGGA_SDK_DIR}/lib")
    endif()
endforeach()
file(GLOB _FRIGGA_ENGINE_LIBRARIES
        "${CMAKE_BINARY_DIR}/*.lib"
        "${CMAKE_BINARY_DIR}/*.a"
        "${CMAKE_BINARY_DIR}/_deps/*-build/*.lib"
        "${CMAKE_BINARY_DIR}/_deps/*-build/*.a"
        "${CMAKE_BINARY_DIR}/_deps/*-build/lib/*.lib"
        "${CMAKE_BINARY_DIR}/_deps/*-build/lib/*.a")
if(_FRIGGA_ENGINE_LIBRARIES)
    file(COPY ${_FRIGGA_ENGINE_LIBRARIES} DESTINATION "${FRIGGA_SDK_DIR}/lib")
endif()

message(STATUS "Frigga gameplay SDK: ${FRIGGA_SDK_DIR}")
