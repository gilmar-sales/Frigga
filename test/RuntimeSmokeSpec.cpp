#include <gtest/gtest.h>

#include <filesystem>

TEST(RuntimeSmoke, PackagedGraphicsShadersArePresent)
{
    const auto resources = std::filesystem::current_path() / "Resources";
    EXPECT_TRUE(std::filesystem::is_regular_file(
        resources / "Shaders/Shadow/depth.vert.spv"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        resources / "Shaders/DeferredCompressed/gbuffer.frag.spv"));
}

TEST(RuntimeSmoke, PackagedAudioAssetsArePresent)
{
    const auto resources =
        std::filesystem::path(FRIGGA_SOURCE_ROOT) / "src/Editor/Resources/ProjectTemplate";
    EXPECT_TRUE(std::filesystem::is_regular_file(
        resources / "Audio/Banks/example.audiobank.json"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        resources / "Audio/Clips/example.wav"));
}

TEST(RuntimeSmoke, RuntimeAndSdkArtifactsArePresent)
{
    const auto root = std::filesystem::current_path();
#if defined(_WIN32)
    EXPECT_TRUE(std::filesystem::is_regular_file(root / "Runtime.exe"));
#else
    EXPECT_TRUE(std::filesystem::is_regular_file(root / "Runtime"));
#endif
    EXPECT_TRUE(std::filesystem::is_regular_file(root / "Sdk/FriggaSdkConfig.cmake"));
}
