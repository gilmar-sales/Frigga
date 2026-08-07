#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Serializable editor preferences (Skirnir Configuration::Bind compatible).
 *
 * Floating fields use double because JsonObjectReader binds numbers to double.
 * Graphics mirrors fra::FreyaOptions (plus a ShadowQuality preset index).
 */
struct AppearancePreferences
{
    int themeIndex = 0;
};

struct GraphicsPreferences
{
    /// Window
    std::string   title      = "Frigga Application";
    std::uint32_t width      = 1280;
    std::uint32_t height     = 720;
    bool          vSync      = true;
    bool          fullscreen = false;
    std::uint32_t sampleCount = 1;
    std::uint32_t frameCount  = 4;

    double clearColorR = 0.0;
    double clearColorG = 0.0;
    double clearColorB = 0.0;
    double clearColorA = 0.0;

    double        drawDistance = 1000.0;
    std::uint32_t maxLights    = 16;
    double        iblIntensity = 0.7;
    double        exposure     = 0.7;
    double        ambientColorR     = 1.0;
    double        ambientColorG     = 1.0;
    double        ambientColorB     = 1.0;
    double        ambientIntensity  = 0.03;

    std::string environmentMapPath =
        "./Resources/Environments/studio_small_09_4k.hdr";
    std::string shaderRoot = "./Resources/Shaders";

    /// 0=Low, 1=Medium, 2=High, 3=Ultra (fra::ShadowQuality).
    int           shadowQuality       = 2;
    std::uint32_t shadowCascadeCount  = 4;
    std::uint32_t shadowMapResolution = 2048;
    double        shadowBias          = 0.002;
    double        shadowLightSize     = 0.03;
    double        shadowMaxSoftness   = 8.0;
    double        shadowMinVisibility = 0.0;
    std::uint32_t maxSpotShadows      = 4;
    std::uint32_t maxPointShadows     = 2;
    std::uint32_t shadowSampleCount   = 16;

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
