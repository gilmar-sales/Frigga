# Populates ${CMAKE_BINARY_DIR}/Sdk for gameplay plugin scaffolding / CI publish.
# Layout mirrors FRIGGA_ROOT + FRIGGA_BUILD paths used by generated CMakeLists.txt:
#   Sdk/src/Frigga/...
#   Sdk/_deps/{freyr,skirnir,glm}-src/...

set(FRIGGA_SDK_DIR "${CMAKE_BINARY_DIR}/Sdk")

set(_FRIGGA_SDK_FREYR "${CMAKE_BINARY_DIR}/_deps/freyr-src")
set(_FRIGGA_SDK_SKIRNIR "${CMAKE_BINARY_DIR}/_deps/skirnir-src")
set(_FRIGGA_SDK_GLM "${CMAKE_BINARY_DIR}/_deps/glm-src")
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

foreach(_dep IN ITEMS "${_FRIGGA_SDK_FREYR}/include/Freyr" "${_FRIGGA_SDK_SKIRNIR}/include/Skirnir"
                      "${_FRIGGA_SDK_GLM}/glm")
    if(NOT EXISTS "${_dep}")
        message(FATAL_ERROR "PackFriggaSdk: missing dependency tree at ${_dep}")
    endif()
endforeach()

file(REMOVE_RECURSE "${FRIGGA_SDK_DIR}")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/src/Frigga/Plugin")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/freyr-src/include")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/skirnir-src/include")
file(MAKE_DIRECTORY "${FRIGGA_SDK_DIR}/_deps/glm-src")

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

file(WRITE "${FRIGGA_SDK_DIR}/src/Frigga/Frigga.hpp" "#pragma once\n// Frigga gameplay plugin SDK\n")
file(WRITE "${FRIGGA_SDK_DIR}/CMakeLists.txt" "# Frigga gameplay plugin SDK (packaged with Editor)\n")

file(COPY "${_FRIGGA_SDK_FREYR}/include/Freyr"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/freyr-src/include")
file(COPY "${_FRIGGA_SDK_SKIRNIR}/include/Skirnir"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/skirnir-src/include")
file(COPY "${_FRIGGA_SDK_GLM}/glm"
     DESTINATION "${FRIGGA_SDK_DIR}/_deps/glm-src")

message(STATUS "Frigga gameplay SDK: ${FRIGGA_SDK_DIR}")
