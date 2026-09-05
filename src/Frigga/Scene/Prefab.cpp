#include <Frigga/Scene/Prefab.hpp>

#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Scene/SceneSerializer.hpp"

#include <cctype>
#include <format>
#include <fstream>
#include <system_error>

#define SIMDJSON_STATIC_REFLECTION 1
#include <simdjson.h>

namespace FRIGGA_NAMESPACE
{
    bool Prefab::IsPrefabExtension(std::string_view extension)
    {
        return AssetRegistry::IsPrefabExtension(extension);
    }

    std::filesystem::path Prefab::DefaultDirectory()
    {
        return AssetRegistry::ResourcesRoot() / "Prefabs";
    }

    std::filesystem::path Prefab::SanitizeFileStem(std::string_view name)
    {
        std::string stem;
        stem.reserve(name.size());
        for(const unsigned char ch : name)
        {
            if(std::isalnum(ch) || ch == '_' || ch == '-')
            {
                stem.push_back(static_cast<char>(ch));
            }
            else if(ch == ' ' || ch == '.')
            {
                if(!stem.empty() && stem.back() != '_')
                {
                    stem.push_back('_');
                }
            }
        }
        while(!stem.empty() && stem.back() == '_')
        {
            stem.pop_back();
        }
        if(stem.empty())
        {
            stem = "Prefab";
        }
        return stem;
    }

    std::filesystem::path Prefab::UniqueAssetPath(const std::filesystem::path &directory,
                                                  std::string_view stem)
    {
        const auto sanitized = SanitizeFileStem(stem).string();
        auto candidate       = directory / (sanitized + ".prefab");
        std::error_code ec;
        for(int suffix = 2; std::filesystem::exists(candidate, ec); ++suffix)
        {
            candidate = directory / std::format("{}_{}.prefab", sanitized, suffix);
        }
        return candidate;
    }

    bool Prefab::Serialize(Scene &scene, fr::Entity root, std::string &outJson)
    {
        return SceneSerializer::SerializePrefab(scene, root, outJson);
    }

    bool Prefab::Instantiate(Scene &scene, std::string_view json, fr::Entity parent,
                             fr::Entity &outRoot)
    {
        return SceneSerializer::InstantiatePrefab(scene, json, parent, outRoot);
    }

    bool Prefab::Save(Scene &scene, fr::Entity root, const std::filesystem::path &path)
    {
        std::string json;
        if(!Serialize(scene, root, json))
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            return false;
        }

        file << json;
        if(!json.empty() && json.back() != '\n')
        {
            file << '\n';
        }
        return static_cast<bool>(file);
    }

    bool Prefab::Load(Scene &scene, const std::filesystem::path &path, fr::Entity parent,
                      fr::Entity &outRoot)
    {
        simdjson::padded_string json;
        if(const auto error = simdjson::padded_string::load(path.string()).get(json); error)
        {
            return false;
        }

        auto relative = AssetRegistry::MakeRelativeToResources(path);
        if(relative.empty())
        {
            relative = path.lexically_normal().generic_string();
        }

        return SceneSerializer::InstantiatePrefab(scene, std::string_view(json.data(), json.size()),
                                                  parent, outRoot, relative.generic_string());
    }
} // namespace FRIGGA_NAMESPACE
