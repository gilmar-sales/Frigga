#include "Editor/Project/ProjectFile.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>

TEST(ProjectFile, SavesAndLoadsPublishBranding)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-project-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    ProjectDescriptor descriptor;
    descriptor.name                  = "Sample";
    descriptor.branding.displayName  = "Sample Game";
    descriptor.branding.executableName = "SampleGame";
    descriptor.branding.publisher     = "Sample Studio";
    descriptor.branding.copyright     = "Copyright (C) Sample Studio";
    descriptor.branding.version       = "2.3.4";
    descriptor.branding.identifier    = "com.sample.game";
    descriptor.branding.iconWindows   = "branding/game.ico";
    descriptor.branding.iconLinux     = "branding/game.png";
    descriptor.branding.iconMacOS     = "branding/game.icns";

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

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}
