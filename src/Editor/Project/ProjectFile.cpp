#include "ProjectFile.hpp"

#include <algorithm>
#include <cctype>
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
            default:
                out << ch;
                break;
            }
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

    bool ExtractJsonIntField(std::string_view text, std::string_view key, int &out)
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
        if(i >= text.size() || (!std::isdigit(static_cast<unsigned char>(text[i])) && text[i] != '-'))
        {
            return false;
        }
        const auto begin = i;
        if(text[i] == '-')
        {
            ++i;
        }
        while(i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
        {
            ++i;
        }
        try
        {
            out = std::stoi(std::string(text.substr(begin, i - begin)));
            return true;
        }
        catch(...)
        {
            return false;
        }
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

    std::string_view ExtractJsonObject(std::string_view text, std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        const auto keyPos        = text.find(needle);
        if(keyPos == std::string_view::npos)
        {
            return {};
        }
        const auto open = text.find('{', keyPos + needle.size());
        if(open == std::string_view::npos)
        {
            return {};
        }
        std::size_t depth = 0;
        for(std::size_t i = open; i < text.size(); ++i)
        {
            if(text[i] == '{')
            {
                ++depth;
            }
            else if(text[i] == '}' && --depth == 0)
            {
                return text.substr(open, i - open + 1);
            }
        }
        return {};
    }

    ModuleSource ParseModuleSource(std::string_view value)
    {
        return value == "user" ? ModuleSource::User : ModuleSource::Project;
    }

    const char *ModuleSourceId(ModuleSource source)
    {
        return source == ModuleSource::User ? "user" : "project";
    }

    void ParseModuleObject(std::string_view object, ProjectModuleEntry &entry)
    {
        ExtractJsonStringField(object, "id", entry.id);
        ExtractJsonStringField(object, "target", entry.target);
        ExtractJsonStringField(object, "library", entry.libraryRelative);
        ExtractJsonBoolField(object, "enabled", entry.enabled);
        std::string source;
        if(ExtractJsonStringField(object, "source", source))
        {
            entry.source = ParseModuleSource(source);
        }
        if(entry.id.empty())
        {
            entry.id = entry.target;
        }
        if(entry.target.empty())
        {
            entry.target = entry.id;
        }
    }

    void ExtractModulesArray(std::string_view text, std::vector<ProjectModuleEntry> &out)
    {
        const auto arrayKey = text.find("\"modules\"");
        if(arrayKey == std::string_view::npos)
        {
            return;
        }
        const auto bracket = text.find('[', arrayKey);
        if(bracket == std::string_view::npos)
        {
            return;
        }
        const auto end = text.find(']', bracket);
        if(end == std::string_view::npos)
        {
            return;
        }
        const auto body = text.substr(bracket + 1, end - bracket - 1);
        std::size_t search = 0;
        while(search < body.size())
        {
            const auto objStart = body.find('{', search);
            if(objStart == std::string_view::npos)
            {
                break;
            }
            const auto objEnd = body.find('}', objStart);
            if(objEnd == std::string_view::npos)
            {
                break;
            }
            ProjectModuleEntry entry;
            ParseModuleObject(body.substr(objStart, objEnd - objStart + 1), entry);
            if(!entry.id.empty() || !entry.target.empty())
            {
                out.push_back(std::move(entry));
            }
            search = objEnd + 1;
        }
    }
} // namespace

bool ProjectFile::Save(const std::filesystem::path &projectFile, const ProjectDescriptor &desc)
{
    ProjectDescriptor written = desc;
    written.SyncGameplayMirror();
    written.EnsureBranding();

    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": " << written.formatVersion << ",\n";
    json << "  \"name\": \"" << EscapeJson(written.name) << "\",\n";
    json << "  \"template\": \"" << EscapeJson(written.TemplateId()) << "\",\n";
    json << "  \"scene\": \"" << EscapeJson(written.sceneRelativePath) << "\",\n";
    json << "  \"publish\": {\n";
    json << "    \"displayName\": \"" << EscapeJson(written.branding.displayName) << "\",\n";
    json << "    \"executableName\": \"" << EscapeJson(written.branding.executableName)
         << "\",\n";
    json << "    \"publisher\": \"" << EscapeJson(written.branding.publisher) << "\",\n";
    json << "    \"copyright\": \"" << EscapeJson(written.branding.copyright) << "\",\n";
    json << "    \"version\": \"" << EscapeJson(written.branding.version) << "\",\n";
    json << "    \"identifier\": \"" << EscapeJson(written.branding.identifier) << "\",\n";
    json << "    \"iconWindows\": \"" << EscapeJson(written.branding.iconWindows.generic_string())
         << "\",\n";
    json << "    \"iconLinux\": \"" << EscapeJson(written.branding.iconLinux.generic_string())
         << "\",\n";
    json << "    \"iconMacOS\": \"" << EscapeJson(written.branding.iconMacOS.generic_string())
         << "\"\n";
    json << "  },\n";
    const auto hasGameplay = std::ranges::any_of(
        written.modules, [](const ProjectModuleEntry &entry) { return entry.IsGameplay(); });
    if(hasGameplay)
    {
        json << "  \"module\": {\n";
        json << "    \"target\": \"" << EscapeJson(written.moduleTarget) << "\",\n";
        json << "    \"library\": \"" << EscapeJson(written.moduleLibraryRelative) << "\"\n";
        json << "  },\n";
    }
    json << "  \"modules\": [\n";
    for(std::size_t i = 0; i < written.modules.size(); ++i)
    {
        const auto &entry = written.modules[i];
        json << "    {\n";
        json << "      \"id\": \"" << EscapeJson(entry.id) << "\",\n";
        json << "      \"target\": \"" << EscapeJson(entry.target) << "\",\n";
        json << "      \"library\": \"" << EscapeJson(entry.libraryRelative) << "\",\n";
        json << "      \"enabled\": " << (entry.enabled ? "true" : "false") << ",\n";
        json << "      \"source\": \"" << ModuleSourceId(entry.source) << "\"\n";
        json << "    }";
        json << (i + 1 < written.modules.size() ? ",\n" : "\n");
    }
    json << "  ],\n";
    json << "  \"engine\": {\n";
    json << "    \"friggaSdk\": \"" << EscapeJson(written.friggaSdk.generic_string()) << "\",\n";
    json << "    \"friggaRoot\": \"" << EscapeJson(written.friggaRoot.generic_string()) << "\",\n";
    json << "    \"friggaBuild\": \"" << EscapeJson(written.friggaBuild.generic_string()) << "\"\n";
    json << "  }\n";
    json << "}\n";

    std::error_code ec;
    std::filesystem::create_directories(projectFile.parent_path(), ec);
    std::ofstream file(projectFile, std::ios::binary | std::ios::trunc);
    if(!file)
    {
        return false;
    }
    file << json.str();
    return static_cast<bool>(file);
}

std::optional<ProjectDescriptor> ProjectFile::Load(const std::filesystem::path &projectFile)
{
    if(!std::filesystem::exists(projectFile))
    {
        return std::nullopt;
    }

    std::ifstream file(projectFile);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    ProjectDescriptor desc;
    if(!ExtractJsonStringField(text, "name", desc.name))
    {
        return std::nullopt;
    }

    int version = ProjectDescriptor::LegacyFormatVersion;
    if(ExtractJsonIntField(text, "version", version))
    {
        desc.formatVersion = version > 0 ? version : ProjectDescriptor::LegacyFormatVersion;
    }
    else
    {
        desc.formatVersion = ProjectDescriptor::LegacyFormatVersion;
    }

    std::string templateId;
    if(ExtractJsonStringField(text, "template", templateId))
    {
        desc.sceneTemplate = ProjectDescriptor::TemplateFromId(templateId);
    }

    ExtractJsonStringField(text, "scene", desc.sceneRelativePath);

    const auto publish = ExtractJsonObject(text, "publish");
    if(!publish.empty())
    {
        ExtractJsonStringField(publish, "displayName", desc.branding.displayName);
        ExtractJsonStringField(publish, "executableName", desc.branding.executableName);
        ExtractJsonStringField(publish, "publisher", desc.branding.publisher);
        ExtractJsonStringField(publish, "copyright", desc.branding.copyright);
        ExtractJsonStringField(publish, "version", desc.branding.version);
        ExtractJsonStringField(publish, "identifier", desc.branding.identifier);
        std::string icon;
        if(ExtractJsonStringField(publish, "iconWindows", icon))
        {
            desc.branding.iconWindows = icon;
        }
        if(ExtractJsonStringField(publish, "iconLinux", icon))
        {
            desc.branding.iconLinux = icon;
        }
        if(ExtractJsonStringField(publish, "iconMacOS", icon))
        {
            desc.branding.iconMacOS = icon;
        }
    }

    // Nested module.library / module.target — keys appear as plain "target"/"library".
    ExtractJsonStringField(text, "target", desc.moduleTarget);
    ExtractJsonStringField(text, "library", desc.moduleLibraryRelative);

    std::string sdk;
    std::string root;
    std::string build;
    ExtractJsonStringField(text, "friggaSdk", sdk);
    ExtractJsonStringField(text, "friggaRoot", root);
    ExtractJsonStringField(text, "friggaBuild", build);
    desc.friggaSdk   = sdk;
    desc.friggaRoot  = root;
    desc.friggaBuild = build;

    ExtractModulesArray(text, desc.modules);
    desc.SyncGameplayMirror();
    desc.EnsureBranding();

    return desc;
}
