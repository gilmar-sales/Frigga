#include "ProjectFile.hpp"

#include <simdjson.h>

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

    simdjson::dom::parser parser;
    simdjson::dom::element document;
    if(const auto result = parser.parse(text).get(document);
       result != simdjson::error_code::SUCCESS)
    {
        return std::nullopt;
    }

    simdjson::dom::object root;
    if(const auto result = document.get_object().get(root);
       result != simdjson::error_code::SUCCESS)
    {
        return std::nullopt;
    }

    auto readString = [](simdjson::dom::object object, std::string_view key,
                         std::string &out, bool required = false) {
        auto field = object.at_key(key);
        if(field.error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            return !required;
        }
        if(field.error() != simdjson::error_code::SUCCESS)
        {
            return false;
        }
        std::string_view value;
        if(field.get_string().get(value) != simdjson::error_code::SUCCESS)
        {
            return false;
        }
        out = value;
        return true;
    };

    auto readInt = [](simdjson::dom::object object, std::string_view key, int &out) {
        auto field = object.at_key(key);
        if(field.error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            return true;
        }
        if(field.error() != simdjson::error_code::SUCCESS)
        {
            return false;
        }
        std::int64_t value = 0;
        if(field.get_int64().get(value) != simdjson::error_code::SUCCESS)
        {
            return false;
        }
        out = static_cast<int>(value);
        return true;
    };

    auto readObject = [](simdjson::dom::object object, std::string_view key,
                         simdjson::dom::object &out, bool &present) {
        auto field = object.at_key(key);
        if(field.error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            present = false;
            return true;
        }
        if(field.error() != simdjson::error_code::SUCCESS)
        {
            return false;
        }
        present = true;
        return field.get_object().get(out) == simdjson::error_code::SUCCESS;
    };

    ProjectDescriptor desc;
    if(!readString(root, "name", desc.name, true))
    {
        return std::nullopt;
    }

    int version = ProjectDescriptor::LegacyFormatVersion;
    if(!readInt(root, "version", version))
    {
        return std::nullopt;
    }
    desc.formatVersion = version > 0 ? version : ProjectDescriptor::LegacyFormatVersion;

    std::string templateId;
    if(!readString(root, "template", templateId) || !readString(root, "scene", desc.sceneRelativePath))
    {
        return std::nullopt;
    }
    if(!templateId.empty())
    {
        desc.sceneTemplate = ProjectDescriptor::TemplateFromId(templateId);
    }

    simdjson::dom::object publish {};
    bool hasPublish = false;
    if(!readObject(root, "publish", publish, hasPublish))
    {
        return std::nullopt;
    }
    if(hasPublish &&
       (!readString(publish, "displayName", desc.branding.displayName) ||
        !readString(publish, "executableName", desc.branding.executableName) ||
        !readString(publish, "publisher", desc.branding.publisher) ||
        !readString(publish, "copyright", desc.branding.copyright) ||
        !readString(publish, "version", desc.branding.version) ||
        !readString(publish, "identifier", desc.branding.identifier)))
    {
        return std::nullopt;
    }
    std::string icon;
    if(hasPublish && !readString(publish, "iconWindows", icon))
    {
        return std::nullopt;
    }
    desc.branding.iconWindows = icon;
    if(hasPublish && !readString(publish, "iconLinux", icon))
    {
        return std::nullopt;
    }
    desc.branding.iconLinux = icon;
    if(hasPublish && !readString(publish, "iconMacOS", icon))
    {
        return std::nullopt;
    }
    desc.branding.iconMacOS = icon;

    simdjson::dom::object module {};
    bool hasModule = false;
    if(!readObject(root, "module", module, hasModule))
    {
        return std::nullopt;
    }
    if(hasModule &&
       (!readString(module, "target", desc.moduleTarget) ||
        !readString(module, "library", desc.moduleLibraryRelative)))
    {
        return std::nullopt;
    }

    simdjson::dom::object engine {};
    bool hasEngine = false;
    if(!readObject(root, "engine", engine, hasEngine))
    {
        return std::nullopt;
    }
    std::string sdk;
    std::string engineRoot;
    std::string build;
    if(hasEngine &&
       (!readString(engine, "friggaSdk", sdk) ||
        !readString(engine, "friggaRoot", engineRoot) ||
        !readString(engine, "friggaBuild", build)))
    {
        return std::nullopt;
    }
    desc.friggaSdk   = sdk;
    desc.friggaRoot  = engineRoot;
    desc.friggaBuild = build;

    auto modulesField = root.at_key("modules");
    if(modulesField.error() == simdjson::error_code::NO_SUCH_FIELD)
    {
        desc.SyncGameplayMirror();
        desc.EnsureBranding();
        return desc;
    }
    if(modulesField.error() != simdjson::error_code::SUCCESS)
    {
        return std::nullopt;
    }
    simdjson::dom::array modules;
    if(modulesField.get_array().get(modules) != simdjson::error_code::SUCCESS)
    {
        return std::nullopt;
    }
    for(const auto moduleElement : modules)
    {
        simdjson::dom::object moduleObject;
        if(moduleElement.get_object().get(moduleObject) != simdjson::error_code::SUCCESS)
        {
            return std::nullopt;
        }

        ProjectModuleEntry entry;
        if(!readString(moduleObject, "id", entry.id) ||
           !readString(moduleObject, "target", entry.target) ||
           !readString(moduleObject, "library", entry.libraryRelative))
        {
            return std::nullopt;
        }
        auto enabled = moduleObject.at_key("enabled");
        if(enabled.error() == simdjson::error_code::SUCCESS &&
           enabled.get_bool().get(entry.enabled) != simdjson::error_code::SUCCESS)
        {
            return std::nullopt;
        }
        std::string source;
        if(!readString(moduleObject, "source", source))
        {
            return std::nullopt;
        }
        entry.source = ParseModuleSource(source);
        if(entry.id.empty())
        {
            entry.id = entry.target;
        }
        if(entry.target.empty())
        {
            entry.target = entry.id;
        }
        if(!entry.id.empty() || !entry.target.empty())
        {
            desc.modules.push_back(std::move(entry));
        }
    }

    desc.SyncGameplayMirror();
    desc.EnsureBranding();

    return desc;
}
