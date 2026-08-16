#include "PluginCatalog.hpp"

#include "ProjectDescriptor.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace
{
    std::string EscapeJson(std::string_view value)
    {
        std::ostringstream out;
        for(const char ch : value)
        {
            if(ch == '"' || ch == '\\')
            {
                out << '\\';
            }
            out << ch;
        }
        return out.str();
    }

    std::string UnescapeJsonString(std::string_view value)
    {
        std::string out;
        out.reserve(value.size());
        for(std::size_t i = 0; i < value.size(); ++i)
        {
            if(value[i] == '\\' && i + 1 < value.size())
            {
                out.push_back(value[i + 1]);
                ++i;
            }
            else
            {
                out.push_back(value[i]);
            }
        }
        return out;
    }

    bool ExtractJsonStringField(std::string_view text, std::string_view key, std::string &out)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        const auto keyPos        = text.find(needle);
        if(keyPos == std::string_view::npos)
        {
            return false;
        }
        const auto colon = text.find(':', keyPos + needle.size());
        if(colon == std::string_view::npos)
        {
            return false;
        }
        const auto quoteOpen = text.find('"', colon + 1);
        if(quoteOpen == std::string_view::npos)
        {
            return false;
        }
        std::size_t i = quoteOpen + 1;
        std::string raw;
        while(i < text.size())
        {
            if(text[i] == '\\' && i + 1 < text.size())
            {
                raw.push_back(text[i]);
                raw.push_back(text[i + 1]);
                i += 2;
                continue;
            }
            if(text[i] == '"')
            {
                out = UnescapeJsonString(raw);
                return true;
            }
            raw.push_back(text[i]);
            ++i;
        }
        return false;
    }

    bool ExtractJsonBoolField(std::string_view text, std::string_view key, bool &out)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        const auto keyPos        = text.find(needle);
        if(keyPos == std::string_view::npos)
        {
            return false;
        }
        const auto colon = text.find(':', keyPos + needle.size());
        if(colon == std::string_view::npos)
        {
            return false;
        }
        std::size_t i = colon + 1;
        while(i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
        {
            ++i;
        }
        if(text.substr(i, 4) == "true")
        {
            out = true;
            return true;
        }
        if(text.substr(i, 5) == "false")
        {
            out = false;
            return true;
        }
        return false;
    }
} // namespace

std::string PluginCatalog::SanitizeId(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for(const char ch : raw)
    {
        if(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')
        {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    while(!out.empty() && (out.front() == '-' || out.front() == '_'))
    {
        out.erase(out.begin());
    }
    if(out.empty())
    {
        return "plugin";
    }
    if(std::isdigit(static_cast<unsigned char>(out.front())))
    {
        out.insert(out.begin(), 'p');
    }
    return out;
}

std::optional<DiscoveredPlugin> PluginCatalog::ReadManifest(const std::filesystem::path &pluginRoot)
{
    const auto path = pluginRoot / ManifestFileName;
    if(!std::filesystem::exists(path))
    {
        return std::nullopt;
    }
    std::ifstream file(path);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    DiscoveredPlugin plugin;
    plugin.root = pluginRoot;
    ExtractJsonStringField(text, "id", plugin.id);
    ExtractJsonStringField(text, "name", plugin.name);
    ExtractJsonStringField(text, "target", plugin.target);
    ExtractJsonStringField(text, "library", plugin.libraryRelative);
    ExtractJsonBoolField(text, "bundled", plugin.bundled);
    if(plugin.id.empty())
    {
        plugin.id = pluginRoot.filename().string();
    }
    if(plugin.target.empty())
    {
        plugin.target = plugin.id;
    }
    if(plugin.name.empty())
    {
        plugin.name = plugin.id;
    }
    if(plugin.libraryRelative.empty())
    {
        plugin.libraryRelative = ProjectDescriptor::DefaultLibraryRelative(plugin.target);
    }
    return plugin;
}

bool PluginCatalog::WriteManifest(const std::filesystem::path &pluginRoot,
                                  const DiscoveredPlugin &plugin)
{
    std::error_code ec;
    std::filesystem::create_directories(pluginRoot, ec);
    std::ostringstream json;
    json << "{\n";
    json << "  \"id\": \"" << EscapeJson(plugin.id) << "\",\n";
    json << "  \"name\": \"" << EscapeJson(plugin.name.empty() ? plugin.id : plugin.name) << "\",\n";
    json << "  \"target\": \"" << EscapeJson(plugin.target) << "\",\n";
    json << "  \"library\": \"" << EscapeJson(plugin.libraryRelative) << "\",\n";
    json << "  \"bundled\": " << (plugin.bundled ? "true" : "false") << "\n";
    json << "}\n";
    std::ofstream file(pluginRoot / ManifestFileName, std::ios::binary | std::ios::trunc);
    if(!file)
    {
        return false;
    }
    file << json.str();
    return static_cast<bool>(file);
}

std::vector<DiscoveredPlugin> PluginCatalog::ScanDirectory(const std::filesystem::path &dir,
                                                           bool bundled)
{
    std::vector<DiscoveredPlugin> result;
    std::error_code ec;
    if(!std::filesystem::is_directory(dir, ec))
    {
        return result;
    }
    for(const auto &entry : std::filesystem::directory_iterator(dir, ec))
    {
        if(ec || !entry.is_directory(ec))
        {
            continue;
        }
        auto plugin = ReadManifest(entry.path());
        if(!plugin)
        {
            continue;
        }
        plugin->bundled = bundled || plugin->bundled;
        result.push_back(std::move(*plugin));
    }
    return result;
}

std::vector<std::filesystem::path> PluginCatalog::BundledPluginSearchDirs(
    const std::filesystem::path &friggaSdk, const std::filesystem::path &friggaRoot,
    const std::filesystem::path &executableDir)
{
    std::vector<std::filesystem::path> dirs;
    std::unordered_set<std::string> seen;
    auto add = [&](std::filesystem::path path) {
        if(path.empty())
        {
            return;
        }
        std::error_code ec;
        if(!std::filesystem::is_directory(path, ec))
        {
            return;
        }
        const auto canonical = std::filesystem::weakly_canonical(path, ec);
        const auto key       = (ec ? path : canonical).string();
        if(!seen.insert(key).second)
        {
            return;
        }
        dirs.push_back(ec ? std::move(path) : canonical);
    };
    // First hit wins in ScanBundled: this Editor's copy, then SDK pack, then source.
    add(executableDir / "Resources" / "plugins");
    add(friggaSdk / "plugins");
    add(friggaRoot / "src" / "Editor" / "Resources" / "plugins");
    return dirs;
}

std::vector<DiscoveredPlugin> PluginCatalog::ScanBundled(const std::filesystem::path &friggaSdk,
                                                         const std::filesystem::path &friggaRoot,
                                                         const std::filesystem::path &executableDir)
{
    std::vector<DiscoveredPlugin> result;
    std::unordered_set<std::string> ids;
    for(const auto &dir : BundledPluginSearchDirs(friggaSdk, friggaRoot, executableDir))
    {
        for(auto &plugin : ScanDirectory(dir, true))
        {
            if(!ids.insert(plugin.id).second)
            {
                continue;
            }
            result.push_back(std::move(plugin));
        }
    }
    return result;
}

bool PluginCatalog::CopyPluginTree(const std::filesystem::path &from, const std::filesystem::path &to,
                                   std::string &error)
{
    std::error_code ec;
    if(!std::filesystem::is_directory(from, ec))
    {
        error = "Plugin folder not found: " + from.string();
        return false;
    }
    if(std::filesystem::exists(to, ec))
    {
        error = "Destination already exists: " + to.string();
        return false;
    }
    std::filesystem::create_directories(to.parent_path(), ec);
    std::filesystem::copy(from, to,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          ec);
    if(ec)
    {
        error = "Failed to copy plugin: " + ec.message();
        return false;
    }
    std::filesystem::remove_all(to / "build", ec);
    return true;
}
