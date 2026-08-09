#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Serializable editor preferences (Skirnir Configuration::Bind compatible).
 *
 * Floating fields use double because JsonObjectReader binds numbers to double.
 * Graphics quality indices match Freya enums: 0=Low … 3=Ultra, 4=Off.
 */
struct AppearancePreferences
{
    int themeIndex = 0;
};

struct GraphicsPreferences
{
    /// Window
    std::string   title       = "Frigga Application";
    std::uint32_t width       = 1280;
    std::uint32_t height      = 720;
    bool          vSync       = true;
    bool          fullscreen  = false;
    std::uint32_t frameCount  = 4;

    double clearColorR = 0.0;
    double clearColorG = 0.0;
    double clearColorB = 0.0;
    double clearColorA = 0.0;

    double        drawDistance     = 1000.0;
    std::uint32_t maxLights        = 16;
    double        iblIntensity     = 0.7;
    double        exposure         = 0.7;
    double        ambientColorR    = 1.0;
    double        ambientColorG    = 1.0;
    double        ambientColorB    = 1.0;
    double        ambientIntensity = 0.03;

    std::string environmentMapPath =
        "./Resources/Environments/studio_small_09_4k.hdr";
    std::string shaderRoot = "./Resources/Shaders";

    /// fra::ShadowQuality / SsaoQuality / TaaQuality / BloomQuality
    int shadowQuality = 2;
    int ssaoQuality   = 2;
    int taaQuality    = 2;
    int bloomQuality  = 2;

    /// Live SSAO knobs (applied after the quality preset).
    double ssaoRadius    = 0.5;
    double ssaoBias      = 0.025;
    double ssaoPower     = 1.5;
    double ssaoIntensity = 0.5;
    /// 0=None, 1=Blurred, 2=Raw (fra::SsaoDebugView)
    int ssaoDebugView = 0;

    bool reverseZ = false;
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
