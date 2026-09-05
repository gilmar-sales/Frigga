#include "RuntimeProject.hpp"

#include <fstream>
#include <sstream>
#include <string_view>

namespace
{
    std::string DefaultLibrary(std::string_view id)
    {
#ifdef _WIN32
        return "Resources/Modules/" + std::string(id) + ".dll";
#elif defined(__APPLE__)
        return "Resources/Modules/lib" + std::string(id) + ".dylib";
#else
        return "Resources/Modules/lib" + std::string(id) + ".so";
#endif
    }

    bool ReadFile(const std::filesystem::path &path, std::string &text)
    {
        std::ifstream file(path, std::ios::binary);
        if(!file)
        {
            return false;
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        text = contents.str();
        return true;
    }

    bool StringField(std::string_view object, std::string_view key, std::string &value)
    {
        const auto keyPos = object.find("\"" + std::string(key) + "\"");
        if(keyPos == std::string_view::npos)
        {
            return false;
        }
        const auto colon = object.find(':', keyPos);
        const auto open  = object.find('"', colon == std::string_view::npos ? keyPos : colon);
        if(open == std::string_view::npos)
        {
            return false;
        }

        std::string raw;
        for(std::size_t i = open + 1; i < object.size(); ++i)
        {
            if(object[i] == '\\' && i + 1 < object.size())
            {
                raw.push_back(object[++i]);
                continue;
            }
            if(object[i] == '"')
            {
                value = std::move(raw);
                return true;
            }
            raw.push_back(object[i]);
        }
        return false;
    }

    bool BoolField(std::string_view object, std::string_view key, bool &value)
    {
        const auto keyPos = object.find("\"" + std::string(key) + "\"");
        if(keyPos == std::string_view::npos)
        {
            return false;
        }
        const auto colon = object.find(':', keyPos);
        if(colon == std::string_view::npos)
        {
            return false;
        }
        const auto begin = object.find_first_not_of(" \t\r\n", colon + 1);
        if(begin == std::string_view::npos)
        {
            return false;
        }
        if(object.substr(begin, 4) == "true")
        {
            value = true;
            return true;
        }
        if(object.substr(begin, 5) == "false")
        {
            value = false;
            return true;
        }
        return false;
    }

    std::string_view ObjectField(std::string_view text, std::string_view key)
    {
        const auto keyPos = text.find("\"" + std::string(key) + "\"");
        if(keyPos == std::string_view::npos)
        {
            return {};
        }
        const auto open = text.find('{', keyPos);
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

    void AddModule(std::string_view object, RuntimeProject &project)
    {
        RuntimeModule module;
        StringField(object, "id", module.id);
        StringField(object, "name", module.name);
        std::string library;
        StringField(object, "library", library);
        BoolField(object, "enabled", module.enabled);

        if(module.id.empty())
        {
            StringField(object, "target", module.id);
        }
        if(module.name.empty())
        {
            module.name = module.id;
        }
        if(library.empty())
        {
            library = DefaultLibrary(module.id);
        }
        module.library = std::move(library);
        if(!module.id.empty())
        {
            project.modules.push_back(std::move(module));
        }
    }
} // namespace

bool RuntimeProject::Load(const std::filesystem::path &projectFile,
                          RuntimeProject &project, std::string &error)
{
    std::string text;
    if(!ReadFile(projectFile, text))
    {
        error = "Unable to read project file: " + projectFile.string();
        return false;
    }

    project = {};
    project.root = projectFile.parent_path();
    if(!StringField(text, "name", project.name))
    {
        error = "Project file has no name: " + projectFile.string();
        return false;
    }
    project.displayName    = project.name;
    project.executableName = project.name;
    const auto publish     = ObjectField(text, "publish");
    if(!publish.empty())
    {
        StringField(publish, "displayName", project.displayName);
        StringField(publish, "executableName", project.executableName);
        StringField(publish, "publisher", project.publisher);
        StringField(publish, "copyright", project.copyright);
        StringField(publish, "version", project.version);
        StringField(publish, "identifier", project.identifier);
    }
    std::string scene;
    if(StringField(text, "scene", scene))
    {
        project.scene = std::move(scene);
    }

    const auto modulesKey = text.find("\"modules\"");
    if(modulesKey != std::string_view::npos)
    {
        const auto open = text.find('[', modulesKey);
        const auto close = open == std::string_view::npos ? std::string_view::npos
                                                           : text.find(']', open);
        if(open != std::string_view::npos && close != std::string_view::npos)
        {
            const auto body = std::string_view(text).substr(open + 1, close - open - 1);
            std::size_t cursor = 0;
            while(cursor < body.size())
            {
                const auto begin = body.find('{', cursor);
                if(begin == std::string_view::npos)
                {
                    break;
                }
                const auto end = body.find('}', begin);
                if(end == std::string_view::npos)
                {
                    error = "Malformed modules array in " + projectFile.string();
                    return false;
                }
                AddModule(body.substr(begin, end - begin + 1), project);
                cursor = end + 1;
            }
        }
    }

    if(project.modules.empty())
    {
        AddModule(R"({"id":"gameplay","name":"Gameplay"})", project);
    }
    return true;
}
