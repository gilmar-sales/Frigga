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
