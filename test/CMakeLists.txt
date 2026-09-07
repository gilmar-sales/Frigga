# Frigga unit tests (GoogleTest)
option(FRIGGA_BUILD_TESTS "Build Frigga unit tests" ON)

if(NOT FRIGGA_BUILD_TESTS)
    return()
endif()

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY "https://github.com/google/googletest.git"
        GIT_TAG "v1.17.0"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(frigga_tests
        AssetManifestSpec.cpp
        DiagnosticsSpec.cpp
        EmptyApp.hpp
        FriModuleSpec.cpp
        InputSpec.cpp
        LateSystemRegistrationSpec.cpp
        EcsLayoutSpec.cpp
        PhysicsSpec.cpp
        SceneSerializerSpec.cpp
        SceneSerializerAudioSpec.cpp
        AnimGraphDefinitionSpec.cpp
        PrefabSpec.cpp
        ModuleHostExportSpec.cpp
        ProjectFileSpec.cpp
        RuntimeProjectSpec.cpp
        RuntimeSmokeSpec.cpp
        ${CMAKE_SOURCE_DIR}/src/Editor/Project/ProjectFile.cpp
        ${CMAKE_SOURCE_DIR}/src/Runtime/RuntimeProject.cpp
)

target_compile_features(frigga_tests PRIVATE cxx_std_26)
target_compile_definitions(frigga_tests PRIVATE
        FRIGGA_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        FRIGGA_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

target_include_directories(frigga_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/Editor
        ${CMAKE_SOURCE_DIR}/src/Runtime
)

target_link_libraries(frigga_tests PRIVATE
        frigga::frigga
        GTest::gtest_main
)

if(UNIX AND NOT APPLE)
    # Gameplay modules leave Frigga symbols UND; host must export them for dlopen.
    target_link_options(frigga_tests PRIVATE -Wl,--export-dynamic)

    add_library(prefab_module_probe SHARED PrefabModuleProbe.cpp)
    target_compile_features(prefab_module_probe PRIVATE cxx_std_26)
    target_compile_options(prefab_module_probe PRIVATE -std=gnu++26)
    target_link_options(prefab_module_probe PRIVATE -Wl,--allow-shlib-undefined)
    set_target_properties(prefab_module_probe PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    add_dependencies(frigga_tests prefab_module_probe Runtime)
    target_compile_definitions(frigga_tests PRIVATE
            FRIGGA_PREFAB_MODULE_PROBE="$<TARGET_FILE:prefab_module_probe>")
endif()

gtest_discover_tests(frigga_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DISCOVERY_TIMEOUT 60
)
