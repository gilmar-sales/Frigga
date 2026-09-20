#pragma once

#include "Frigga/Macro.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    inline constexpr std::string_view kGraphicsConfigFileName = "graphics.json";

    /// Project-local Freya graphics settings (Runtime loads `graphics.json`).
    /// Quality fields use Freya preset names: Low, Medium, High, Ultra, Off.
    struct GraphicsConfig
    {
        int version = 1;

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
        std::uint32_t maxLights        = 64;
        double        iblIntensity     = 0.7;
        double        exposure         = 0.7;
        double        ambientColorR    = 1.0;
        double        ambientColorG    = 1.0;
        double        ambientColorB    = 1.0;
        double        ambientIntensity = 0.03;

        std::string environmentMapPath =
            "./Resources/Environments/studio_small_09_4k.hdr";
        std::string shaderRoot = "./Resources/Shaders";

        std::string shadowQuality    = "High";
        std::string ssaoQuality      = "High";
        std::string taaQuality       = "High";
        std::string bloomQuality     = "High";

        double ssaoRadius    = 0.5;
        double ssaoBias      = 0.025;
        double ssaoPower     = 1.5;
        double ssaoIntensity = 0.5;
        /// fra::DeferredDebugView ordinal (0=Lit/None … 11=Shadows).
        int deferredDebugView = 0;

        bool reverseZ = false;

        std::string animationQuality = "High";
    };

    [[nodiscard]] GraphicsConfig MakeDefaultGraphicsConfig();

} // namespace FRIGGA_NAMESPACE
