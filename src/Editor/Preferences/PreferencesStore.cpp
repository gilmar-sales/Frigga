#include "PreferencesStore.hpp"

#include <fstream>
#include <sstream>

namespace
{
    std::string EscapeJson(std::string_view value)
    {
        std::ostringstream out;
        for(const char ch : value)
        {
            switch(ch)
            {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << ch;
                break;
            }
        }
        return out.str();
    }
} // namespace

void PreferencesStore::Configure(skr::ConfigurationBuilder &configurationBuilder,
                                 const std::filesystem::path &path)
{
    if(std::filesystem::exists(path))
    {
        configurationBuilder.AddJsonFile(path);
    }
}

skr::Arc<EditorPreferences> PreferencesStore::Load(const std::filesystem::path &path)
{
    auto configurationBuilder = skr::ConfigurationBuilder();
    Configure(configurationBuilder, path);
    return configurationBuilder.Build()->Bind<EditorPreferences>();
}

void PreferencesStore::Save(const EditorPreferences &preferences,
                            const std::filesystem::path &path)
{
    const auto &g = preferences.graphics;
    const auto &e = preferences.ecs;

    std::ostringstream json;
    json << std::boolalpha;
    json << "{\n";
    json << "  \"appearance\": {\n";
    json << "    \"themeIndex\": " << preferences.appearance.themeIndex << "\n";
    json << "  },\n";
    json << "  \"graphics\": {\n";
    json << "    \"title\": \"" << EscapeJson(g.title) << "\",\n";
    json << "    \"width\": " << g.width << ",\n";
    json << "    \"height\": " << g.height << ",\n";
    json << "    \"vSync\": " << g.vSync << ",\n";
    json << "    \"fullscreen\": " << g.fullscreen << ",\n";
    json << "    \"sampleCount\": " << g.sampleCount << ",\n";
    json << "    \"frameCount\": " << g.frameCount << ",\n";
    json << "    \"clearColorR\": " << g.clearColorR << ",\n";
    json << "    \"clearColorG\": " << g.clearColorG << ",\n";
    json << "    \"clearColorB\": " << g.clearColorB << ",\n";
    json << "    \"clearColorA\": " << g.clearColorA << ",\n";
    json << "    \"drawDistance\": " << g.drawDistance << ",\n";
    json << "    \"maxLights\": " << g.maxLights << ",\n";
    json << "    \"iblIntensity\": " << g.iblIntensity << ",\n";
    json << "    \"exposure\": " << g.exposure << ",\n";
    json << "    \"ambientColorR\": " << g.ambientColorR << ",\n";
    json << "    \"ambientColorG\": " << g.ambientColorG << ",\n";
    json << "    \"ambientColorB\": " << g.ambientColorB << ",\n";
    json << "    \"ambientIntensity\": " << g.ambientIntensity << ",\n";
    json << "    \"environmentMapPath\": \"" << EscapeJson(g.environmentMapPath)
         << "\",\n";
    json << "    \"shaderRoot\": \"" << EscapeJson(g.shaderRoot) << "\",\n";
    json << "    \"shadowQuality\": " << g.shadowQuality << ",\n";
    json << "    \"shadowCascadeCount\": " << g.shadowCascadeCount << ",\n";
    json << "    \"shadowMapResolution\": " << g.shadowMapResolution << ",\n";
    json << "    \"shadowBias\": " << g.shadowBias << ",\n";
    json << "    \"shadowLightSize\": " << g.shadowLightSize << ",\n";
    json << "    \"shadowMaxSoftness\": " << g.shadowMaxSoftness << ",\n";
    json << "    \"shadowMinVisibility\": " << g.shadowMinVisibility << ",\n";
    json << "    \"maxSpotShadows\": " << g.maxSpotShadows << ",\n";
    json << "    \"maxPointShadows\": " << g.maxPointShadows << ",\n";
    json << "    \"shadowSampleCount\": " << g.shadowSampleCount << ",\n";
    json << "    \"reverseZ\": " << g.reverseZ << ",\n";
    json << "    \"enableSsao\": " << g.enableSsao << ",\n";
    json << "    \"enableTaa\": " << g.enableTaa << ",\n";
    json << "    \"enableBloom\": " << g.enableBloom << "\n";
    json << "  },\n";
    json << "  \"ecs\": {\n";
    json << "    \"maxEntities\": " << e.maxEntities << ",\n";
    json << "    \"archetypeChunkCapacity\": " << e.archetypeChunkCapacity << ",\n";
    json << "    \"threadCount\": " << e.threadCount << "\n";
    json << "  }\n";
    json << "}\n";

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << json.str();
}
