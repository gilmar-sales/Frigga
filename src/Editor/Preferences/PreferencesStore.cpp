#include "PreferencesStore.hpp"

#include <fstream>
#include <sstream>
#include <string>

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

    void WriteViewportQuality(std::ostringstream &json, const char *key,
                               const ViewportQualityPreferences &q)
    {
        json << "    \"" << key << "\": {\n";
        json << "      \"shadowQuality\": " << q.shadowQuality << ",\n";
        json << "      \"ssaoQuality\": " << q.ssaoQuality << ",\n";
        json << "      \"taaQuality\": " << q.taaQuality << ",\n";
        json << "      \"bloomQuality\": " << q.bloomQuality << "\n";
        json << "    }";
    }

    void MigrateLegacyViewportQualities(GraphicsPreferences &graphics,
                                        const std::filesystem::path &path)
    {
        if(!std::filesystem::exists(path))
        {
            return;
        }

        std::ifstream file(path);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string text = buffer.str();
        if(text.find("\"editorViewport\"") != std::string::npos)
        {
            return;
        }

        // Old preferences.json only had flat quality fields — seed both viewports.
        graphics.editorViewport = {
            .shadowQuality = graphics.shadowQuality,
            .ssaoQuality   = graphics.ssaoQuality,
            .taaQuality    = graphics.taaQuality,
            .bloomQuality  = graphics.bloomQuality,
        };
        graphics.gameplayViewport = graphics.editorViewport;
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
    auto preferences = configurationBuilder.Build()->Bind<EditorPreferences>();
    MigrateLegacyViewportQualities(preferences->graphics, path);
    // Keep flat aliases mirrored to gameplay for any leftover readers.
    preferences->graphics.shadowQuality = preferences->graphics.gameplayViewport.shadowQuality;
    preferences->graphics.ssaoQuality   = preferences->graphics.gameplayViewport.ssaoQuality;
    preferences->graphics.taaQuality    = preferences->graphics.gameplayViewport.taaQuality;
    preferences->graphics.bloomQuality  = preferences->graphics.gameplayViewport.bloomQuality;
    return preferences;
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
    WriteViewportQuality(json, "editorViewport", g.editorViewport);
    json << ",\n";
    WriteViewportQuality(json, "gameplayViewport", g.gameplayViewport);
    json << ",\n";
    // Legacy flat aliases (gameplay) for older tools / soft compat.
    json << "    \"shadowQuality\": " << g.gameplayViewport.shadowQuality << ",\n";
    json << "    \"ssaoQuality\": " << g.gameplayViewport.ssaoQuality << ",\n";
    json << "    \"taaQuality\": " << g.gameplayViewport.taaQuality << ",\n";
    json << "    \"bloomQuality\": " << g.gameplayViewport.bloomQuality << ",\n";
    json << "    \"ssaoRadius\": " << g.ssaoRadius << ",\n";
    json << "    \"ssaoBias\": " << g.ssaoBias << ",\n";
    json << "    \"ssaoPower\": " << g.ssaoPower << ",\n";
    json << "    \"ssaoIntensity\": " << g.ssaoIntensity << ",\n";
    json << "    \"ssaoDebugView\": " << g.ssaoDebugView << ",\n";
    json << "    \"reverseZ\": " << g.reverseZ << "\n";
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
