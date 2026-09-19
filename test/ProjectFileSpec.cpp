#include "Editor/Project/ProjectEnginePaths.hpp"
#include "Editor/Project/ProjectFile.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    std::filesystem::path MakeTempRoot(std::string_view prefix)
    {
        return std::filesystem::temp_directory_path() /
               (std::string(prefix) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    }
} // namespace

TEST(ProjectFile, SavesAndLoadsPublishBranding)
{
    const auto root = MakeTempRoot("frigga-project");
    std::filesystem::create_directories(root);

    ProjectDescriptor descriptor;
    descriptor.name                    = "Sample";
    descriptor.branding.displayName    = "Sample Game";
    descriptor.branding.executableName = "SampleGame";
    descriptor.branding.publisher      = "Sample Studio";
    descriptor.branding.copyright      = "Copyright (C) Sample Studio";
    descriptor.branding.version        = "2.3.4";
    descriptor.branding.identifier     = "com.sample.game";
    descriptor.branding.iconWindows    = "branding/game.ico";
    descriptor.branding.iconLinux      = "branding/game.png";
    descriptor.branding.iconMacOS      = "branding/game.icns";
    descriptor.friggaSdk               = "/home/other/Frigga/build/Sdk";
    descriptor.friggaRoot              = "/home/other/Frigga";
    descriptor.friggaBuild             = "/home/other/Frigga/build";

    const auto projectFile = root / ProjectFile::FileName;
    ASSERT_TRUE(ProjectFile::Save(projectFile, descriptor));

    const auto loaded = ProjectFile::Load(projectFile);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->branding.displayName, "Sample Game");
    EXPECT_EQ(loaded->branding.executableName, "SampleGame");
    EXPECT_EQ(loaded->branding.publisher, "Sample Studio");
    EXPECT_EQ(loaded->branding.version, "2.3.4");
    EXPECT_EQ(loaded->branding.identifier, "com.sample.game");
    EXPECT_EQ(loaded->branding.iconWindows, "branding/game.ico");
    // Absolute host paths must not persist in frigga.project.
    EXPECT_TRUE(loaded->friggaSdk.empty());
    EXPECT_TRUE(loaded->friggaRoot.empty());
    EXPECT_TRUE(loaded->friggaBuild.empty());

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(ProjectFile, ParsesEscapedValuesAndNestedEngineFields)
{
    const auto root = MakeTempRoot("frigga-project-structured");
    std::filesystem::create_directories(root);
    const auto projectFile = root / ProjectFile::FileName;
    {
        std::ofstream file(projectFile);
        file << R"json({
  "version": 5,
  "name": "Project {Production} \"A\"",
  "template": "3d",
  "scene": "Scenes/main.json",
  "publish": {
    "displayName": "Game",
    "executableName": "Game",
    "publisher": "Studio",
    "copyright": "Copyright",
    "version": "1.0.0",
    "identifier": "com.studio.game"
  },
  "module": {
    "target": "gameplay",
    "library": "build/libgameplay.so"
  },
  "modules": [
    {
      "id": "gameplay",
      "target": "gameplay",
      "library": "build/libgameplay.so",
      "enabled": true,
      "source": "project"
    }
  ],
  "engine": {
    "friggaSdk": "/sdk",
    "friggaRoot": "/root",
    "friggaBuild": "/build"
  }
})json";
    }

    const auto loaded = ProjectFile::Load(projectFile);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->name, "Project {Production} \"A\"");
    // Legacy files may still contain absolute paths; Load keeps them for rediscovery.
    EXPECT_EQ(loaded->friggaSdk, "/sdk");
    EXPECT_EQ(loaded->friggaRoot, "/root");
    EXPECT_EQ(loaded->friggaBuild, "/build");
    ASSERT_EQ(loaded->modules.size(), 1u);
    EXPECT_EQ(loaded->modules[0].libraryRelative, "build/libgameplay.so");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(ProjectFile, RejectsMalformedJson)
{
    const auto root = MakeTempRoot("frigga-project-invalid");
    std::filesystem::create_directories(root);
    const auto projectFile = root / ProjectFile::FileName;
    {
        std::ofstream file(projectFile);
        file << R"json({"name":"Broken","modules":[{"id":"gameplay"}])json";
    }

    EXPECT_FALSE(ProjectFile::Load(projectFile));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(ProjectEnginePaths, RefreshReplacesUnusableLinuxHints)
{
    ProjectDescriptor desc;
    desc.friggaSdk   = "/home/gilmar/dev/Frigga/build/Sdk";
    desc.friggaRoot  = "/home/gilmar/dev/Frigga";
    desc.friggaBuild = "/home/gilmar/dev/Frigga/build";
    desc.modules.push_back(ProjectModuleEntry {
        .id = "gameplay", .target = "gameplay", .libraryRelative = "build/libgameplay.so"});

    const auto localRoot = MakeTempRoot("frigga-sdk-fake");
    const auto localSdk  = localRoot / "Sdk";
    const auto localBuild = localRoot / "build";
    std::filesystem::create_directories(localSdk / "include/Frigga/Module");
    std::filesystem::create_directories(localSdk / "include/Freyr");
    std::filesystem::create_directories(localSdk / "cmake");
    std::filesystem::create_directories(localBuild);
    {
        std::ofstream(localSdk / "include/Frigga/Module/frigga_module.h") << "#pragma once\n";
        std::ofstream(localSdk / "cmake/FriggaSdk.cmake") << "# sdk\n";
        std::ofstream(localSdk / "include/Freyr/Freyr.hpp") << "#pragma once\n";
    }

    EXPECT_FALSE(IsUsableFriggaSdk(desc.friggaSdk));
    EXPECT_TRUE(EffectiveFriggaSdk(desc).empty());

    RefreshEnginePaths(desc, localSdk, localRoot, localBuild);
    EXPECT_EQ(desc.friggaSdk, localSdk);
    // Fake root is not an engine tree; FillMissing falls back to the usable Sdk.
    EXPECT_EQ(desc.friggaRoot, localSdk);
    EXPECT_EQ(desc.friggaBuild, localBuild);

    NormalizeModuleLibraryPaths(desc);
    EXPECT_EQ(desc.modules[0].libraryRelative, ProjectDescriptor::DefaultLibraryRelative("gameplay"));
    EXPECT_EQ(desc.moduleLibraryRelative, ProjectDescriptor::DefaultLibraryRelative("gameplay"));

    std::error_code ec;
    std::filesystem::remove_all(localRoot, ec);
}
