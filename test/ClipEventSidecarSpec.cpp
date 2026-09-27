#include <Frigga/Animation/ClipEventSidecar.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    std::filesystem::path UniqueTempDir(std::string_view prefix)
    {
        const auto dir = std::filesystem::temp_directory_path() /
                         (std::string(prefix) +
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir);
        return dir;
    }
} // namespace

TEST(ClipEventSidecar, RelativePathKeepsModelDirectory)
{
    const auto relative = fg::ClipEventSidecarRelativePath("Models/Fox.glb");
    EXPECT_EQ(relative.generic_string(), "Models/Fox.anim-events.json");
}

TEST(ClipEventSidecar, RoundTripsEvents)
{
    const auto dir  = UniqueTempDir("frigga-clip-events-");
    const auto file = dir / "Fox.anim-events.json";

    const fg::ClipEventMap saved {
        {"Survey", {{"Hit", 0.42f}, {"Turn", 1.0f}}},
        {"Walk", {}},
    };
    std::string error;
    ASSERT_TRUE(fg::SaveClipEventSidecar(file, saved, &error)) << error;

    fg::ClipEventMap loaded;
    ASSERT_TRUE(fg::LoadClipEventSidecar(file, loaded, &error)) << error;
    ASSERT_EQ(loaded.size(), 2u);
    ASSERT_EQ(loaded.at("Survey").size(), 2u);
    EXPECT_EQ(loaded.at("Survey")[0].name, "Hit");
    EXPECT_FLOAT_EQ(loaded.at("Survey")[0].timeSec, 0.42f);
    EXPECT_EQ(loaded.at("Survey")[1].name, "Turn");
    EXPECT_FLOAT_EQ(loaded.at("Survey")[1].timeSec, 1.0f);
    EXPECT_TRUE(loaded.at("Walk").empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(ClipEventSidecar, LoadsIntegerTimes)
{
    const auto dir  = UniqueTempDir("frigga-clip-events-int-");
    const auto file = dir / "Fox.anim-events.json";
    {
        std::ofstream out(file);
        out << "{\"version\":1,\"events\":{\"Idle\":[{\"name\":\"Tap\",\"time\":1}]}}";
    }

    fg::ClipEventMap loaded;
    std::string error;
    ASSERT_TRUE(fg::LoadClipEventSidecar(file, loaded, &error)) << error;
    ASSERT_EQ(loaded.at("Idle").size(), 1u);
    EXPECT_FLOAT_EQ(loaded.at("Idle")[0].timeSec, 1.0f);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(ClipEventSidecar, RejectsMalformedJson)
{
    const auto dir  = UniqueTempDir("frigga-clip-events-bad-");
    const auto file = dir / "Fox.anim-events.json";
    {
        std::ofstream out(file);
        out << "{\"version\":1,\"events\":[";
    }

    fg::ClipEventMap loaded;
    std::string error;
    EXPECT_FALSE(fg::LoadClipEventSidecar(file, loaded, &error));
    EXPECT_FALSE(error.empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(ClipEventSidecar, AppliesOverridesOnlyToKnownClips)
{
    std::vector<fra::AnimationClip> clips {
        {.name = "Idle", .duration = 2.0f, .events = {{"Old", 0.1f}}},
        {.name = "Walk", .duration = 1.0f},
    };

    const fg::ClipEventMap overrides {
        {"Idle", {{"New", 1.5f}}},
        {"Missing", {{"Ghost", 0.5f}}},
    };
    fg::ApplyClipEventOverrides(clips, overrides);

    ASSERT_EQ(clips[0].events.size(), 1u);
    EXPECT_EQ(clips[0].events[0].name, "New");
    EXPECT_TRUE(clips[1].events.empty());
}

TEST(ClipEventSidecar, EmptyOverrideClearsImportedEvents)
{
    std::vector<fra::AnimationClip> clips {
        {.name = "Idle", .duration = 2.0f, .events = {{"Old", 0.1f}}},
    };

    fg::ApplyClipEventOverrides(clips, fg::ClipEventMap {{"Idle", {}}});
    EXPECT_TRUE(clips[0].events.empty());
}

TEST(ClipEventSidecar, NormalizeClampsAndSorts)
{
    std::vector<fra::AnimationEvent> events {{"B", 9.0f}, {"A", -1.0f}, {"C", 1.0f}};
    fg::NormalizeClipEvents(events, 2.0f);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].name, "A");
    EXPECT_FLOAT_EQ(events[0].timeSec, 0.0f);
    EXPECT_EQ(events[1].name, "C");
    EXPECT_FLOAT_EQ(events[2].timeSec, 2.0f);
}
