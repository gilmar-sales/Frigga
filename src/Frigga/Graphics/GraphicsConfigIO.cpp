#include "Frigga/Graphics/GraphicsConfigIO.hpp"

#define SIMDJSON_STATIC_REFLECTION 1
#include <simdjson.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr int kGraphicsConfigVersion = 1;

        struct GraphicsConfigDto
        {
            int64_t     version          = kGraphicsConfigVersion;
            int64_t     width            = 1280;
            int64_t     height           = 720;
            bool        vSync            = true;
            bool        fullscreen       = false;
            int64_t     frameCount       = 4;
            double      clearColorR      = 0.0;
            double      clearColorG      = 0.0;
            double      clearColorB      = 0.0;
            double      clearColorA      = 0.0;
            double      drawDistance     = 1000.0;
            int64_t     maxLights        = 64;
            double      iblIntensity     = 0.7;
            double      exposure         = 0.7;
            double      ambientColorR    = 1.0;
            double      ambientColorG    = 1.0;
            double      ambientColorB    = 1.0;
            double      ambientIntensity = 0.03;
            std::string environmentMapPath =
                "./Resources/Environments/studio_small_09_4k.hdr";
            std::string shaderRoot         = "./Resources/Shaders";
            std::string shadowQuality      = "High";
            std::string ssaoQuality        = "High";
            std::string taaQuality         = "High";
            std::string bloomQuality       = "High";
            double      ssaoRadius         = 0.5;
            double      ssaoBias           = 0.025;
            double      ssaoPower          = 1.5;
            double      ssaoIntensity      = 0.5;
            int64_t     deferredDebugView  = 0;
            bool        reverseZ           = false;
            std::string animationQuality   = "High";
        };

        [[nodiscard]] std::string NormalizeQualityName(std::string_view name)
        {
            std::string out;
            out.reserve(name.size());
            for(const unsigned char ch : name)
            {
                if(ch == ' ' || ch == '_' || ch == '-')
                {
                    continue;
                }
                out.push_back(static_cast<char>(std::tolower(ch)));
            }
            return out;
        }

        [[nodiscard]] bool ParseQualityName(std::string_view name, std::string &canonical,
                                            std::string *error, std::string_view field)
        {
            const auto key = NormalizeQualityName(name);
            if(key == "low")
            {
                canonical = "Low";
                return true;
            }
            if(key == "medium" || key == "med")
            {
                canonical = "Medium";
                return true;
            }
            if(key == "high")
            {
                canonical = "High";
                return true;
            }
            if(key == "ultra")
            {
                canonical = "Ultra";
                return true;
            }
            if(key == "off" || key == "none" || key == "disabled")
            {
                canonical = "Off";
                return true;
            }
            if(error)
            {
                *error = std::string(field) +
                         " must be Low, Medium, High, Ultra, or Off";
            }
            return false;
        }

        [[nodiscard]] std::string CanonicalQuality(std::string_view name)
        {
            std::string canonical = "High";
            (void)ParseQualityName(name, canonical, nullptr, {});
            return canonical;
        }

        template <typename Quality>
        [[nodiscard]] Quality QualityFromName(std::string_view name)
        {
            const auto key = NormalizeQualityName(name);
            if(key == "low")
            {
                return Quality::Low;
            }
            if(key == "medium" || key == "med")
            {
                return Quality::Medium;
            }
            if(key == "ultra")
            {
                return Quality::Ultra;
            }
            if(key == "off" || key == "none" || key == "disabled")
            {
                return Quality::Off;
            }
            return Quality::High;
        }

        [[nodiscard]] std::uint32_t ClampU32(int64_t value, std::uint32_t fallback)
        {
            if(value <= 0)
            {
                return fallback;
            }
            return static_cast<std::uint32_t>(
                std::min(value, static_cast<int64_t>(std::numeric_limits<std::uint32_t>::max())));
        }

        [[nodiscard]] bool ApplyField(std::string_view key, simdjson::ondemand::value value,
                                      GraphicsConfig &out, std::string *error)
        {
            auto asInt = [&](int minV, int maxV, int &dest) -> bool {
                int64_t number = dest;
                if(value.get_int64().get(number))
                {
                    if(error)
                    {
                        *error = std::string(key) + " must be an integer";
                    }
                    return false;
                }
                dest = static_cast<int>(
                    std::clamp(number, static_cast<int64_t>(minV), static_cast<int64_t>(maxV)));
                return true;
            };
            auto asU32 = [&](std::uint32_t &dest) -> bool {
                int64_t number = dest;
                if(value.get_int64().get(number))
                {
                    if(error)
                    {
                        *error = std::string(key) + " must be an integer";
                    }
                    return false;
                }
                dest = ClampU32(number, dest);
                return true;
            };
            auto asBool = [&](bool &dest) -> bool {
                bool flag = dest;
                if(value.get_bool().get(flag))
                {
                    if(error)
                    {
                        *error = std::string(key) + " must be a boolean";
                    }
                    return false;
                }
                dest = flag;
                return true;
            };
            auto asDouble = [&](double &dest) -> bool {
                double number = dest;
                if(value.get_double().get(number))
                {
                    if(error)
                    {
                        *error = std::string(key) + " must be a number";
                    }
                    return false;
                }
                dest = number;
                return true;
            };
            auto asString = [&](std::string &dest) -> bool {
                std::string_view text;
                if(value.get_string().get(text))
                {
                    if(error)
                    {
                        *error = std::string(key) + " must be a string";
                    }
                    return false;
                }
                dest.assign(text);
                return true;
            };

            auto asQuality = [&](std::string &dest) -> bool {
                std::string_view text;
                if(value.get_string().get(text))
                {
                    if(error)
                    {
                        *error = std::string(key) +
                                 " must be Low, Medium, High, Ultra, or Off";
                    }
                    return false;
                }
                return ParseQualityName(text, dest, error, key);
            };

            if(key == "version")
            {
                return asInt(1, 1024, out.version);
            }
            if(key == "width")
            {
                return asU32(out.width);
            }
            if(key == "height")
            {
                return asU32(out.height);
            }
            if(key == "vSync")
            {
                return asBool(out.vSync);
            }
            if(key == "fullscreen")
            {
                return asBool(out.fullscreen);
            }
            if(key == "frameCount")
            {
                return asU32(out.frameCount);
            }
            if(key == "clearColorR")
            {
                return asDouble(out.clearColorR);
            }
            if(key == "clearColorG")
            {
                return asDouble(out.clearColorG);
            }
            if(key == "clearColorB")
            {
                return asDouble(out.clearColorB);
            }
            if(key == "clearColorA")
            {
                return asDouble(out.clearColorA);
            }
            if(key == "drawDistance")
            {
                return asDouble(out.drawDistance);
            }
            if(key == "maxLights")
            {
                return asU32(out.maxLights);
            }
            if(key == "iblIntensity")
            {
                return asDouble(out.iblIntensity);
            }
            if(key == "exposure")
            {
                return asDouble(out.exposure);
            }
            if(key == "ambientColorR")
            {
                return asDouble(out.ambientColorR);
            }
            if(key == "ambientColorG")
            {
                return asDouble(out.ambientColorG);
            }
            if(key == "ambientColorB")
            {
                return asDouble(out.ambientColorB);
            }
            if(key == "ambientIntensity")
            {
                return asDouble(out.ambientIntensity);
            }
            if(key == "environmentMapPath")
            {
                return asString(out.environmentMapPath);
            }
            if(key == "shaderRoot")
            {
                return asString(out.shaderRoot);
            }
            if(key == "shadowQuality")
            {
                return asQuality(out.shadowQuality);
            }
            if(key == "ssaoQuality")
            {
                return asQuality(out.ssaoQuality);
            }
            if(key == "taaQuality")
            {
                return asQuality(out.taaQuality);
            }
            if(key == "bloomQuality")
            {
                return asQuality(out.bloomQuality);
            }
            if(key == "ssaoRadius")
            {
                return asDouble(out.ssaoRadius);
            }
            if(key == "ssaoBias")
            {
                return asDouble(out.ssaoBias);
            }
            if(key == "ssaoPower")
            {
                return asDouble(out.ssaoPower);
            }
            if(key == "ssaoIntensity")
            {
                return asDouble(out.ssaoIntensity);
            }
            if(key == "deferredDebugView")
            {
                return asInt(0, 11, out.deferredDebugView);
            }
            if(key == "reverseZ")
            {
                return asBool(out.reverseZ);
            }
            if(key == "animationQuality")
            {
                return asQuality(out.animationQuality);
            }
            return true;
        }

        [[nodiscard]] GraphicsConfigDto ToDto(const GraphicsConfig &config)
        {
            GraphicsConfigDto document {};
            document.version =
                config.version <= 0 ? kGraphicsConfigVersion : config.version;
            document.width              = config.width;
            document.height             = config.height;
            document.vSync              = config.vSync;
            document.fullscreen         = config.fullscreen;
            document.frameCount         = config.frameCount;
            document.clearColorR        = config.clearColorR;
            document.clearColorG        = config.clearColorG;
            document.clearColorB        = config.clearColorB;
            document.clearColorA        = config.clearColorA;
            document.drawDistance       = config.drawDistance;
            document.maxLights          = config.maxLights;
            document.iblIntensity       = config.iblIntensity;
            document.exposure           = config.exposure;
            document.ambientColorR      = config.ambientColorR;
            document.ambientColorG      = config.ambientColorG;
            document.ambientColorB      = config.ambientColorB;
            document.ambientIntensity   = config.ambientIntensity;
            document.environmentMapPath = config.environmentMapPath;
            document.shaderRoot         = config.shaderRoot;
            document.shadowQuality      = CanonicalQuality(config.shadowQuality);
            document.ssaoQuality        = CanonicalQuality(config.ssaoQuality);
            document.taaQuality         = CanonicalQuality(config.taaQuality);
            document.bloomQuality       = CanonicalQuality(config.bloomQuality);
            document.ssaoRadius         = config.ssaoRadius;
            document.ssaoBias           = config.ssaoBias;
            document.ssaoPower          = config.ssaoPower;
            document.ssaoIntensity      = config.ssaoIntensity;
            document.deferredDebugView  = std::clamp(config.deferredDebugView, 0, 11);
            document.reverseZ           = config.reverseZ;
            document.animationQuality   = CanonicalQuality(config.animationQuality);
            return document;
        }

        [[nodiscard]] bool SerializeToJson(const GraphicsConfig &config, std::string &outJson,
                                           std::string *error)
        {
            const auto document = ToDto(config);
            outJson.clear();
            if(const auto err = simdjson::to_json(document, outJson); err)
            {
                if(error)
                {
                    *error = simdjson::error_message(err);
                }
                return false;
            }
            if(!outJson.empty() && outJson.back() != '\n')
            {
                outJson.push_back('\n');
            }
            return true;
        }
    } // namespace

    std::string SerializeGraphicsConfig(const GraphicsConfig &config)
    {
        std::string json;
        if(!SerializeToJson(config, json, nullptr))
        {
            return "{}\n";
        }
        return json;
    }

    bool ParseGraphicsConfig(std::string_view json, GraphicsConfig &out, std::string *error)
    {
        GraphicsConfig parsed = MakeDefaultGraphicsConfig();

        simdjson::ondemand::parser parser;
        simdjson::padded_string padded(json);
        simdjson::ondemand::document doc;
        if(parser.iterate(padded).get(doc))
        {
            if(error)
            {
                *error = "invalid JSON";
            }
            return false;
        }

        simdjson::ondemand::object root;
        if(doc.get_object().get(root))
        {
            if(error)
            {
                *error = "expected JSON object";
            }
            return false;
        }

        for(auto field : root)
        {
            std::string_view key;
            if(field.unescaped_key().get(key))
            {
                continue;
            }
            simdjson::ondemand::value value;
            if(field.value().get(value))
            {
                if(error)
                {
                    *error = "invalid field value for " + std::string(key);
                }
                return false;
            }
            if(!ApplyField(key, value, parsed, error))
            {
                return false;
            }
        }

        out = std::move(parsed);
        return true;
    }

    bool LoadGraphicsConfigFile(const std::filesystem::path &path, GraphicsConfig &out,
                                std::string *error)
    {
        simdjson::padded_string json;
        if(const auto err = simdjson::padded_string::load(path.string()).get(json); err)
        {
            if(error)
            {
                *error = simdjson::error_message(err);
            }
            return false;
        }
        return ParseGraphicsConfig(std::string_view(json.data(), json.size()), out, error);
    }

    bool SaveGraphicsConfigFile(const std::filesystem::path &path, const GraphicsConfig &config,
                                std::string *error)
    {
        std::string json;
        if(!SerializeToJson(config, json, error))
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            if(error)
            {
                *error = "Failed to open graphics.json for writing";
            }
            return false;
        }
        file << json;
        return static_cast<bool>(file);
    }

    bool EnsureGraphicsConfigFile(const std::filesystem::path &path, const GraphicsConfig &config,
                                  std::string *error)
    {
        std::error_code ec;
        if(std::filesystem::exists(path, ec))
        {
            return true;
        }
        return SaveGraphicsConfigFile(path, config, error);
    }

    std::string_view GraphicsQualityLabel(int quality)
    {
        switch(std::clamp(quality, 0, 4))
        {
            case 0:
                return "Low";
            case 1:
                return "Medium";
            case 2:
                return "High";
            case 3:
                return "Ultra";
            default:
                return "Off";
        }
    }

    void ApplyGraphicsConfig(fra::FreyaOptionsBuilder &builder, const GraphicsConfig &config)
    {
        const auto deferredDebugView = static_cast<fra::DeferredDebugView>(
            std::clamp(config.deferredDebugView, 0, 11));

        builder.SetWidth(config.width)
            .SetHeight(config.height)
            .SetFullscreen(config.fullscreen)
            .SetVSync(config.vSync)
            .SetFrameCount(config.frameCount)
            .SetClearColor(glm::vec4 {static_cast<float>(config.clearColorR),
                                      static_cast<float>(config.clearColorG),
                                      static_cast<float>(config.clearColorB),
                                      static_cast<float>(config.clearColorA)})
            .SetDrawDistance(static_cast<float>(config.drawDistance))
            .SetMaxLights(config.maxLights)
            .SetIblIntensity(static_cast<float>(config.iblIntensity))
            .SetExposure(static_cast<float>(config.exposure))
            .SetAmbient(glm::vec3(static_cast<float>(config.ambientColorR),
                                  static_cast<float>(config.ambientColorG),
                                  static_cast<float>(config.ambientColorB)),
                        static_cast<float>(config.ambientIntensity))
            .SetEnvironmentMapPath(config.environmentMapPath)
            .SetShaderRoot(config.shaderRoot)
            .SetShadowQuality(QualityFromName<fra::ShadowQuality>(config.shadowQuality))
            .SetSsaoQuality(QualityFromName<fra::SsaoQuality>(config.ssaoQuality))
            .SetTaaQuality(QualityFromName<fra::TaaQuality>(config.taaQuality))
            .SetBloomQuality(QualityFromName<fra::BloomQuality>(config.bloomQuality))
            .SetSsaoRadius(static_cast<float>(config.ssaoRadius))
            .SetSsaoBias(static_cast<float>(config.ssaoBias))
            .SetSsaoPower(static_cast<float>(config.ssaoPower))
            .SetSsaoIntensity(static_cast<float>(config.ssaoIntensity))
            .SetDeferredDebugView(deferredDebugView)
            .WithReverseZ(config.reverseZ)
            .SetAnimationQuality(
                QualityFromName<fra::AnimationQuality>(config.animationQuality));
    }

} // namespace FRIGGA_NAMESPACE
