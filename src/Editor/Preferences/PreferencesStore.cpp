#include "PreferencesStore.hpp"

#include "../Paths/EditorPaths.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    std::filesystem::path ResolvePreferencesPath(const std::filesystem::path &path)
    {
        if(!path.empty())
        {
            return path;
        }
        return PreferencesStore::DefaultPath();
    }

    /// One-shot: copy cwd preferences.json into the OS preferred dir when migrating.
    void MigrateLegacyPreferencesIfNeeded(const std::filesystem::path &target)
    {
        if(std::filesystem::exists(target))
        {
            return;
        }

        const auto legacy = std::filesystem::current_path() / "preferences.json";
        if(!std::filesystem::exists(legacy))
        {
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
        std::filesystem::copy_file(legacy, target, ec);
    }

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

    std::string UnescapeJsonString(std::string_view value)
    {
        std::string out;
        out.reserve(value.size());
        for(std::size_t i = 0; i < value.size(); ++i)
        {
            if(value[i] == '\\' && i + 1 < value.size())
            {
                switch(value[i + 1])
                {
                case '"':
                    out.push_back('"');
                    ++i;
                    break;
                case '\\':
                    out.push_back('\\');
                    ++i;
                    break;
                case 'n':
                    out.push_back('\n');
                    ++i;
                    break;
                case 'r':
                    out.push_back('\r');
                    ++i;
                    break;
                case 't':
                    out.push_back('\t');
                    ++i;
                    break;
                default:
                    out.push_back(value[i]);
                    break;
                }
            }
            else
            {
                out.push_back(value[i]);
            }
        }
        return out;
    }

    bool ExtractJsonStringField(std::string_view object, std::string_view key, std::string &out)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        const auto keyPos        = object.find(needle);
        if(keyPos == std::string_view::npos)
        {
            return false;
        }
        const auto colon = object.find(':', keyPos + needle.size());
        if(colon == std::string_view::npos)
        {
            return false;
        }
        const auto quoteOpen = object.find('"', colon + 1);
        if(quoteOpen == std::string_view::npos)
        {
            return false;
        }
        std::size_t i = quoteOpen + 1;
        std::string raw;
        while(i < object.size())
        {
            if(object[i] == '\\' && i + 1 < object.size())
            {
                raw.push_back(object[i]);
                raw.push_back(object[i + 1]);
                i += 2;
                continue;
            }
            if(object[i] == '"')
            {
                out = UnescapeJsonString(raw);
                return true;
            }
            raw.push_back(object[i]);
            ++i;
        }
        return false;
    }

    void LoadRecentProjects(EditorPreferences &preferences, const std::filesystem::path &path)
    {
        preferences.recentProjects.clear();
        if(!std::filesystem::exists(path))
        {
            return;
        }

        std::ifstream file(path);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string text = buffer.str();

        const auto arrayKey = text.find("\"recentProjects\"");
        if(arrayKey == std::string::npos)
        {
            return;
        }
        const auto bracket = text.find('[', arrayKey);
        if(bracket == std::string::npos)
        {
            return;
        }
        const auto endBracket = text.find(']', bracket);
        if(endBracket == std::string::npos)
        {
            return;
        }

        std::size_t cursor = bracket + 1;
        while(cursor < endBracket)
        {
            const auto objStart = text.find('{', cursor);
            if(objStart == std::string::npos || objStart >= endBracket)
            {
                break;
            }
            const auto objEnd = text.find('}', objStart);
            if(objEnd == std::string::npos || objEnd > endBracket)
            {
                break;
            }

            const std::string_view object(text.data() + objStart, objEnd - objStart + 1);
            RecentProjectEntry entry;
            if(ExtractJsonStringField(object, "path", entry.path) &&
               ExtractJsonStringField(object, "name", entry.name))
            {
                ExtractJsonStringField(object, "openedAt", entry.openedAt);
                preferences.recentProjects.push_back(std::move(entry));
            }
            cursor = objEnd + 1;
        }
    }

    std::string NowIso8601()
    {
        using clock = std::chrono::system_clock;
        const auto now = clock::now();
        const auto tt  = clock::to_time_t(now);
        std::tm tm {};
#if defined(_WIN32)
        gmtime_s(&tm, &tt);
#else
        gmtime_r(&tt, &tm);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return buf;
    }
} // namespace

void PreferencesStore::Configure(skr::ConfigurationBuilder &configurationBuilder,
                                 const std::filesystem::path &path)
{
    const auto resolved = ResolvePreferencesPath(path);
    MigrateLegacyPreferencesIfNeeded(resolved);
    if(std::filesystem::exists(resolved))
    {
        configurationBuilder.AddJsonFile(resolved);
    }
}

std::filesystem::path PreferencesStore::DefaultPath()
{
    EditorPaths::EnsureDirectories();
    return EditorPaths::PreferencesFile();
}

skr::Arc<EditorPreferences> PreferencesStore::Load(const std::filesystem::path &path)
{
    const auto resolved = ResolvePreferencesPath(path);
    EditorPaths::EnsureDirectories();
    MigrateLegacyPreferencesIfNeeded(resolved);

    auto configurationBuilder = skr::ConfigurationBuilder();
    if(std::filesystem::exists(resolved))
    {
        std::ifstream file(resolved);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string text = buffer.str();
        // Strip recentProjects so Skirnir Bind (no vector support) stays happy.
        const auto recentKey = text.find("\"recentProjects\"");
        if(recentKey != std::string::npos)
        {
            const auto bracket = text.find('[', recentKey);
            const auto endBracket =
                bracket == std::string::npos ? std::string::npos : text.find(']', bracket);
            if(bracket != std::string::npos && endBracket != std::string::npos)
            {
                auto eraseBegin = recentKey;
                while(eraseBegin > 0 && (text[eraseBegin - 1] == ' ' || text[eraseBegin - 1] == '\n' ||
                                         text[eraseBegin - 1] == ','))
                {
                    --eraseBegin;
                }
                auto eraseEnd = endBracket + 1;
                if(eraseEnd < text.size() && text[eraseEnd] == ',')
                {
                    ++eraseEnd;
                }
                text.erase(eraseBegin, eraseEnd - eraseBegin);
            }
        }
        configurationBuilder.AddJsonString(text);
    }
    auto preferences = configurationBuilder.Build()->Bind<EditorPreferences>();
    MigrateLegacyViewportQualities(preferences->graphics, resolved);
    // Keep flat aliases mirrored to gameplay for any leftover readers.
    preferences->graphics.shadowQuality = preferences->graphics.gameplayViewport.shadowQuality;
    preferences->graphics.ssaoQuality   = preferences->graphics.gameplayViewport.ssaoQuality;
    preferences->graphics.taaQuality    = preferences->graphics.gameplayViewport.taaQuality;
    preferences->graphics.bloomQuality  = preferences->graphics.gameplayViewport.bloomQuality;
    LoadRecentProjects(*preferences, resolved);
    return preferences;
}

void PreferencesStore::TouchRecentProject(EditorPreferences &preferences,
                                          const std::filesystem::path &projectFile,
                                          const std::string &name)
{
    std::string canonical = projectFile.string();
    std::error_code ec;
    const auto weakly = std::filesystem::weakly_canonical(projectFile, ec);
    if(!ec)
    {
        canonical = weakly.string();
    }

    preferences.recentProjects.erase(
        std::remove_if(preferences.recentProjects.begin(), preferences.recentProjects.end(),
                       [&](const RecentProjectEntry &entry) {
                           std::error_code entryEc;
                           const auto entryPath =
                               std::filesystem::weakly_canonical(entry.path, entryEc);
                           if(!entryEc)
                           {
                               return entryPath.string() == canonical;
                           }
                           return entry.path == canonical || entry.path == projectFile.string();
                       }),
        preferences.recentProjects.end());

    preferences.recentProjects.insert(
        preferences.recentProjects.begin(),
        RecentProjectEntry {.path = canonical, .name = name, .openedAt = NowIso8601()});

    if(preferences.recentProjects.size() > MaxRecentProjects)
    {
        preferences.recentProjects.resize(MaxRecentProjects);
    }
}

void PreferencesStore::RemoveRecentProject(EditorPreferences &preferences,
                                           const std::filesystem::path &projectFile)
{
    std::string canonical = projectFile.string();
    std::error_code ec;
    const auto weakly = std::filesystem::weakly_canonical(projectFile, ec);
    if(!ec)
    {
        canonical = weakly.string();
    }

    preferences.recentProjects.erase(
        std::remove_if(preferences.recentProjects.begin(), preferences.recentProjects.end(),
                       [&](const RecentProjectEntry &entry) {
                           std::error_code entryEc;
                           const auto entryPath =
                               std::filesystem::weakly_canonical(entry.path, entryEc);
                           if(!entryEc)
                           {
                               return entryPath.string() == canonical;
                           }
                           return entry.path == canonical || entry.path == projectFile.string();
                       }),
        preferences.recentProjects.end());
}

void PreferencesStore::Save(const EditorPreferences &preferences,
                            const std::filesystem::path &path)
{
    const auto resolved = ResolvePreferencesPath(path);
    EditorPaths::EnsureDirectories();

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
    json << "    \"reverseZ\": " << g.reverseZ << ",\n";
    json << "    \"animationQuality\": " << g.animationQuality << "\n";
    json << "  },\n";
    json << "  \"ecs\": {\n";
    json << "    \"maxEntities\": " << e.maxEntities << ",\n";
    json << "    \"archetypeChunkCapacity\": " << e.archetypeChunkCapacity << ",\n";
    json << "    \"threadCount\": " << e.threadCount << "\n";
    json << "  },\n";
    json << "  \"tools\": {\n";
    json << "    \"codeEditorCommand\": \"" << EscapeJson(preferences.tools.codeEditorCommand)
         << "\"\n";
    json << "  },\n";
    json << "  \"recentProjects\": [\n";
    for(std::size_t i = 0; i < preferences.recentProjects.size(); ++i)
    {
        const auto &entry = preferences.recentProjects[i];
        json << "    {\n";
        json << "      \"path\": \"" << EscapeJson(entry.path) << "\",\n";
        json << "      \"name\": \"" << EscapeJson(entry.name) << "\",\n";
        json << "      \"openedAt\": \"" << EscapeJson(entry.openedAt) << "\"\n";
        json << "    }";
        if(i + 1 < preferences.recentProjects.size())
        {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    std::ofstream file(resolved, std::ios::binary | std::ios::trunc);
    file << json.str();
}
