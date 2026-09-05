#include "RuntimeProject.hpp"

#include <simdjson.h>

#include <fstream>
#include <sstream>
#include <string_view>

namespace
{
    std::string DefaultLibrary(std::string_view id)
    {
#ifdef _WIN32
        return "Modules/" + std::string(id) + ".dll";
#elif defined(__APPLE__)
        return "Modules/lib" + std::string(id) + ".dylib";
#else
        return "Modules/lib" + std::string(id) + ".so";
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

    bool ReadString(simdjson::dom::object object, std::string_view key, std::string &value,
                    std::string &error, bool required = false)
    {
        auto field = object.at_key(key);
        if(field.error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            if(required)
            {
                error = "Missing required string field: " + std::string(key);
            }
            return !required;
        }
        if(field.error() != simdjson::error_code::SUCCESS)
        {
            error = "Unable to read field '" + std::string(key) +
                    "': " + simdjson::error_message(field.error());
            return false;
        }

        std::string_view parsed;
        const auto result = field.get_string().get(parsed);
        if(result != simdjson::error_code::SUCCESS)
        {
            error = "Field '" + std::string(key) + "' must be a string: " +
                    simdjson::error_message(result);
            return false;
        }
        value = parsed;
        return true;
    }

    bool ReadBool(simdjson::dom::object object, std::string_view key, bool &value,
                  std::string &error)
    {
        auto field = object.at_key(key);
        if(field.error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            return true;
        }
        if(field.error() != simdjson::error_code::SUCCESS)
        {
            error = "Unable to read field '" + std::string(key) +
                    "': " + simdjson::error_message(field.error());
            return false;
        }

        const auto result = field.get_bool().get(value);
        if(result != simdjson::error_code::SUCCESS)
        {
            error = "Field '" + std::string(key) + "' must be a boolean: " +
                    simdjson::error_message(result);
            return false;
        }
        return true;
    }

    bool ReadPublish(simdjson::dom::object root, RuntimeProject &project, std::string &error)
    {
        auto field = root.at_key("publish");
        if(field.error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            return true;
        }
        if(field.error() != simdjson::error_code::SUCCESS)
        {
            error = "Unable to read field 'publish': " +
                    std::string(simdjson::error_message(field.error()));
            return false;
        }

        simdjson::dom::object publish;
        const auto objectResult = field.get_object().get(publish);
        if(objectResult != simdjson::error_code::SUCCESS)
        {
            error = "Field 'publish' must be an object: " +
                    std::string(simdjson::error_message(objectResult));
            return false;
        }

        if(!ReadString(publish, "displayName", project.displayName, error) ||
           !ReadString(publish, "executableName", project.executableName, error) ||
           !ReadString(publish, "publisher", project.publisher, error) ||
           !ReadString(publish, "copyright", project.copyright, error) ||
           !ReadString(publish, "version", project.version, error) ||
           !ReadString(publish, "identifier", project.identifier, error))
        {
            return false;
        }
        return true;
    }

    bool AddModule(simdjson::dom::element element, RuntimeProject &project, std::string &error)
    {
        simdjson::dom::object object;
        const auto objectResult = element.get_object().get(object);
        if(objectResult != simdjson::error_code::SUCCESS)
        {
            error = "Each entry in 'modules' must be an object: " +
                    std::string(simdjson::error_message(objectResult));
            return false;
        }

        RuntimeModule module;
        if(!ReadString(object, "id", module.id, error) ||
           !ReadString(object, "name", module.name, error))
        {
            return false;
        }
        std::string library;
        if(!ReadString(object, "library", library, error) ||
           !ReadBool(object, "enabled", module.enabled, error))
        {
            return false;
        }

        if(module.id.empty() && !ReadString(object, "target", module.id, error))
        {
            return false;
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
        return true;
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

    simdjson::dom::parser parser;
    simdjson::dom::element document;
    const auto parseResult = parser.parse(text).get(document);
    if(parseResult != simdjson::error_code::SUCCESS)
    {
        error = "Invalid project JSON: " + std::string(simdjson::error_message(parseResult));
        return false;
    }

    simdjson::dom::object root;
    const auto objectResult = document.get_object().get(root);
    if(objectResult != simdjson::error_code::SUCCESS)
    {
        error = "Project file root must be an object: " +
                std::string(simdjson::error_message(objectResult));
        return false;
    }

    project = {};
    project.root = projectFile.parent_path();
    if(!ReadString(root, "name", project.name, error, true))
    {
        return false;
    }
    project.displayName    = project.name;
    project.executableName = project.name;
    if(!ReadPublish(root, project, error))
    {
        return false;
    }
    std::string scene;
    if(!ReadString(root, "scene", scene, error))
    {
        return false;
    }
    if(!scene.empty())
    {
        project.scene = std::move(scene);
    }

    auto modulesField = root.at_key("modules");
    if(modulesField.error() == simdjson::error_code::NO_SUCH_FIELD)
    {
        return true;
    }
    if(modulesField.error() != simdjson::error_code::SUCCESS)
    {
        error = "Unable to read field 'modules': " +
                std::string(simdjson::error_message(modulesField.error()));
        return false;
    }

    simdjson::dom::array modules;
    const auto arrayResult = modulesField.get_array().get(modules);
    if(arrayResult != simdjson::error_code::SUCCESS)
    {
        error = "Field 'modules' must be an array: " +
                std::string(simdjson::error_message(arrayResult));
        return false;
    }
    for(const auto module : modules)
    {
        if(!AddModule(module, project, error))
        {
            return false;
        }
    }

    return true;
}
