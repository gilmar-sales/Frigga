#include <Frigga/Graphics/GraphicsConfig.hpp>
#include <Frigga/Graphics/GraphicsConfigIO.hpp>

#include <gtest/gtest.h>

#include <filesystem>

TEST(GraphicsConfig, SerializeRoundTripPreservesDefaults)
{
    const auto original = fg::MakeDefaultGraphicsConfig();
    const auto json     = fg::SerializeGraphicsConfig(original);
    fg::GraphicsConfig parsed;
    std::string error;
    ASSERT_TRUE(fg::ParseGraphicsConfig(json, parsed, &error)) << error;

    EXPECT_EQ(parsed.version, original.version);
    EXPECT_EQ(parsed.width, original.width);
    EXPECT_EQ(parsed.height, original.height);
    EXPECT_EQ(parsed.vSync, original.vSync);
    EXPECT_EQ(parsed.fullscreen, original.fullscreen);
    EXPECT_EQ(parsed.frameCount, original.frameCount);
    EXPECT_NEAR(parsed.clearColorR, original.clearColorR, 1e-9);
    EXPECT_NEAR(parsed.drawDistance, original.drawDistance, 1e-9);
    EXPECT_EQ(parsed.maxLights, original.maxLights);
    EXPECT_NEAR(parsed.iblIntensity, original.iblIntensity, 1e-9);
    EXPECT_NEAR(parsed.exposure, original.exposure, 1e-9);
    EXPECT_NEAR(parsed.ambientIntensity, original.ambientIntensity, 1e-9);
    EXPECT_EQ(parsed.environmentMapPath, original.environmentMapPath);
    EXPECT_EQ(parsed.shaderRoot, original.shaderRoot);
    EXPECT_EQ(parsed.shadowQuality, "High");
    EXPECT_EQ(parsed.ssaoQuality, "High");
    EXPECT_EQ(parsed.taaQuality, "High");
    EXPECT_EQ(parsed.bloomQuality, "High");
    EXPECT_NEAR(parsed.ssaoRadius, original.ssaoRadius, 1e-9);
    EXPECT_EQ(parsed.deferredDebugView, original.deferredDebugView);
    EXPECT_EQ(parsed.reverseZ, original.reverseZ);
    EXPECT_EQ(parsed.animationQuality, "High");
    EXPECT_NE(json.find("\"shadowQuality\":\"High\""), std::string::npos);
}

TEST(GraphicsConfig, ParseOverridesSelectedFields)
{
    constexpr std::string_view json = R"({
      "width": 1920,
      "height": 1080,
      "vSync": false,
      "fullscreen": true,
      "shadowQuality": "Ultra",
      "reverseZ": true,
      "animationQuality": "medium"
    })";

    fg::GraphicsConfig parsed = fg::MakeDefaultGraphicsConfig();
    std::string error;
    ASSERT_TRUE(fg::ParseGraphicsConfig(json, parsed, &error)) << error;

    EXPECT_EQ(parsed.width, 1920u);
    EXPECT_EQ(parsed.height, 1080u);
    EXPECT_FALSE(parsed.vSync);
    EXPECT_TRUE(parsed.fullscreen);
    EXPECT_EQ(parsed.shadowQuality, "Ultra");
    EXPECT_TRUE(parsed.reverseZ);
    EXPECT_EQ(parsed.animationQuality, "Medium");
    EXPECT_EQ(parsed.maxLights, 64u);
    EXPECT_EQ(parsed.ssaoQuality, "High");
}

TEST(GraphicsConfig, RejectsUnknownQuality)
{
    constexpr std::string_view json = R"({ "shadowQuality": "Potato" })";
    fg::GraphicsConfig parsed;
    std::string error;
    EXPECT_FALSE(fg::ParseGraphicsConfig(json, parsed, &error));
    EXPECT_FALSE(error.empty());
}

TEST(GraphicsConfig, EnsureWritesOnlyWhenMissing)
{
    const auto dir =
        std::filesystem::temp_directory_path() / "frigga-graphics-config-ensure";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    const auto path = dir / "graphics.json";

    auto first = fg::MakeDefaultGraphicsConfig();
    first.width = 1600;
    std::string error;
    ASSERT_TRUE(fg::EnsureGraphicsConfigFile(path, first, &error)) << error;
    ASSERT_TRUE(std::filesystem::exists(path));

    auto second = fg::MakeDefaultGraphicsConfig();
    second.width = 800;
    ASSERT_TRUE(fg::EnsureGraphicsConfigFile(path, second, &error)) << error;

    fg::GraphicsConfig loaded;
    ASSERT_TRUE(fg::LoadGraphicsConfigFile(path, loaded, &error)) << error;
    EXPECT_EQ(loaded.width, 1600u);

    std::filesystem::remove_all(dir, ec);
}
