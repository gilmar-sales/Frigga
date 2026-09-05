#include "RuntimeProject.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

TEST(RuntimeProject, LoadsStartupSceneAndEnabledModules)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-runtime-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto projectFile = root / "frigga.project";
    {
        std::ofstream file(projectFile);
        file << R"json({
  "name": "Published",
  "publish": {
    "displayName": "Published Game",
    "executableName": "PublishedGame",
    "publisher": "Studio",
    "copyright": "Copyright (C) Studio",
    "version": "3.2.1",
    "identifier": "com.studio.published"
  },
  "scene": "scenes/intro.json",
  "modules": [
    {"id": "camera", "name": "Camera", "library": "build/libcamera.so", "enabled": true},
    {"id": "debug", "library": "build/libdebug.so", "enabled": false}
  ]
})json";
    }

    RuntimeProject project;
    std::string error;
    ASSERT_TRUE(RuntimeProject::Load(projectFile, project, error)) << error;
    EXPECT_EQ(project.name, "Published");
    EXPECT_EQ(project.displayName, "Published Game");
    EXPECT_EQ(project.executableName, "PublishedGame");
    EXPECT_EQ(project.publisher, "Studio");
    EXPECT_EQ(project.version, "3.2.1");
    EXPECT_EQ(project.identifier, "com.studio.published");
    EXPECT_EQ(project.ScenePath(), root / "scenes/intro.json");
    ASSERT_EQ(project.modules.size(), 2u);
    EXPECT_EQ(project.modules[0].library, "build/libcamera.so");
    EXPECT_TRUE(project.modules[0].enabled);
    EXPECT_FALSE(project.modules[1].enabled);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(RuntimeProject, AllowsProjectsWithoutModules)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-runtime-empty-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto projectFile = root / "frigga.project";
    {
        std::ofstream file(projectFile);
        file << R"json({
  "name": "EmptyProject",
  "scene": "scenes/main.json",
  "modules": []
})json";
    }

    RuntimeProject project;
    std::string error;
    ASSERT_TRUE(RuntimeProject::Load(projectFile, project, error)) << error;
    EXPECT_TRUE(project.modules.empty());

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(RuntimeProject, ParsesEscapedStringsAndNestedModuleData)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-runtime-escaped-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto projectFile = root / "frigga.project";
    {
        std::ofstream file(projectFile);
        file << R"json({
  "name": "Game with {braces} and \"quotes\"",
  "publish": {
    "displayName": "Display {Production}",
    "executableName": "Game",
    "publisher": "Studio",
    "copyright": "Copyright \"Studio\"",
    "version": "1.0.0",
    "identifier": "com.studio.game"
  },
  "scene": "Scenes/main.json",
  "modules": [
    {
      "id": "gameplay",
      "name": "Gameplay {Core}",
      "library": "Modules/libgameplay.so",
      "enabled": true
    }
  ]
})json";
    }

    RuntimeProject project;
    std::string error;
    ASSERT_TRUE(RuntimeProject::Load(projectFile, project, error)) << error;
    EXPECT_EQ(project.name, "Game with {braces} and \"quotes\"");
    EXPECT_EQ(project.displayName, "Display {Production}");
    ASSERT_EQ(project.modules.size(), 1u);
    EXPECT_EQ(project.modules[0].name, "Gameplay {Core}");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(RuntimeProject, RejectsMalformedJsonAndInvalidFieldTypes)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-runtime-invalid-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    const auto malformed = root / "malformed.project";
    {
        std::ofstream file(malformed);
        file << R"json({"name":"Broken","modules":[{"id":"gameplay"}])json";
    }

    RuntimeProject project;
    std::string error;
    EXPECT_FALSE(RuntimeProject::Load(malformed, project, error));
    EXPECT_NE(error.find("Invalid project JSON"), std::string::npos);

    const auto wrongType = root / "wrong-type.project";
    {
        std::ofstream file(wrongType);
        file << R"json({"name":"Broken","modules":{}})json";
    }

    error.clear();
    EXPECT_FALSE(RuntimeProject::Load(wrongType, project, error));
    EXPECT_NE(error.find("modules"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}
