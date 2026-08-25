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
        EmptyApp.hpp
        FriPluginModuleSpec.cpp
        InputSpec.cpp
        LateSystemRegistrationSpec.cpp
        EcsLayoutSpec.cpp
        PhysicsSpec.cpp
        SceneSerializerSpec.cpp
        SceneSerializerAudioSpec.cpp
        AnimGraphDefinitionSpec.cpp
        PrefabSpec.cpp
)

target_compile_features(frigga_tests PRIVATE cxx_std_26)
target_compile_definitions(frigga_tests PRIVATE
        FRIGGA_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_include_directories(frigga_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(frigga_tests PRIVATE
        frigga::frigga
        GTest::gtest_main
)

gtest_discover_tests(frigga_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DISCOVERY_TIMEOUT 60
)
