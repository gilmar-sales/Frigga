#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Serializable editor preferences (Skirnir Configuration::Bind compatible).
 *
 * Floating fields use double because JsonObjectReader binds numbers to double.
 */
struct AppearancePreferences
{
    int themeIndex = 0;
};

struct GraphicsPreferences
{
    bool          vSync             = true;
    bool          fullscreen        = false;
    std::uint32_t sampleCount       = 1;
    double        drawDistance      = 1000.0;
    double        iblIntensity      = 0.7;
    double        exposure          = 0.7;
    double        ambientColorR     = 1.0;
    double        ambientColorG     = 1.0;
    double        ambientColorB     = 1.0;
    double        ambientIntensity  = 0.03;

    /// Applied on next launch.
    std::uint32_t frameCount         = 4;
    std::uint32_t maxLights          = 16;
    std::string   environmentMapPath =
        "./Resources/Environments/studio_small_09_4k.hdr";
    bool reverseZ    = false;
    bool enableSsao  = true;
    bool enableTaa   = true;
    bool enableBloom = true;
};

struct EcsPreferences
{
    /// Applied on next launch.
    std::uint64_t maxEntities            = 1024ull * 1024ull;
    std::uint64_t archetypeChunkCapacity = 512;
    std::uint64_t threadCount            = 4;
};

struct EditorPreferences
{
    AppearancePreferences appearance;
    GraphicsPreferences   graphics;
    EcsPreferences        ecs;
};
