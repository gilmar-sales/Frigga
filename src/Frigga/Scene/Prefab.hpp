#pragma once

#include "Frigga/Scene/Scene.hpp"

#include <Freyr/Freyr.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    class Prefab
    {
      public:
        /// Serialize `root` and its descendants to JSON (no editor camera).
        static bool Serialize(Scene &scene, fr::Entity root, std::string &outJson);

        /// Instantiate a prefab JSON blob into `scene`. Roots are parented to `parent`
        /// when it is a valid entity. `outRoot` is the first root entity created.
        static bool Instantiate(Scene &scene, std::string_view json, fr::Entity parent,
                                fr::Entity &outRoot);

        static bool Save(Scene &scene, fr::Entity root, const std::filesystem::path &path);
        static bool Load(Scene &scene, const std::filesystem::path &path, fr::Entity parent,
                         fr::Entity &outRoot);

        [[nodiscard]] static bool IsPrefabExtension(std::string_view extension);
        [[nodiscard]] static std::filesystem::path DefaultDirectory();
        [[nodiscard]] static std::filesystem::path SanitizeFileStem(std::string_view name);
    };

} // namespace FRIGGA_NAMESPACE
