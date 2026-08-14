#include "ProjectFile.hpp"

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
} // namespace

bool ProjectFile::Save(const std::filesystem::path &projectFile, const ProjectDescriptor &desc)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": " << desc.formatVersion << ",\n";
    json << "  \"name\": \"" << EscapeJson(desc.name) << "\",\n";
    json << "  \"template\": \"" << EscapeJson(desc.TemplateId()) << "\",\n";
    json << "  \"scene\": \"" << EscapeJson(desc.sceneRelativePath) << "\",\n";
    json << "  \"plugin\": {\n";
    json << "    \"target\": \"" << EscapeJson(desc.pluginTarget) << "\",\n";
    json << "    \"library\": \"" << EscapeJson(desc.pluginLibraryRelative) << "\"\n";
    json << "  },\n";
    json << "  \"engine\": {\n";
    json << "    \"friggaSdk\": \"" << EscapeJson(desc.friggaSdk.generic_string()) << "\",\n";
    json << "    \"friggaRoot\": \"" << EscapeJson(desc.friggaRoot.generic_string()) << "\",\n";
    json << "    \"friggaBuild\": \"" << EscapeJson(desc.friggaBuild.generic_string()) << "\"\n";
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

    // Nested plugin.library / plugin.target — keys appear as plain "target"/"library".
    ExtractJsonStringField(text, "target", desc.pluginTarget);
    ExtractJsonStringField(text, "library", desc.pluginLibraryRelative);

    std::string sdk;
    std::string root;
    std::string build;
    ExtractJsonStringField(text, "friggaSdk", sdk);
    ExtractJsonStringField(text, "friggaRoot", root);
    ExtractJsonStringField(text, "friggaBuild", build);
    desc.friggaSdk   = sdk;
    desc.friggaRoot  = root;
    desc.friggaBuild = build;

    return desc;
}
