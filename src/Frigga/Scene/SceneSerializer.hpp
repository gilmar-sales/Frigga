#pragma once

#include "Frigga/Scene/Scene.hpp"

#include <Freyr/Freyr.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    class SceneSerializer
    {
      public:
        static bool Serialize(Scene &scene, std::string &outJson);
        static bool Deserialize(Scene &scene, std::string_view json);

        static bool Save(Scene &scene, const std::filesystem::path &path);
        static bool Load(Scene &scene, const std::filesystem::path &path);

        /// Serialize one component from @p entity. @p kind is a built-in field name
        /// (transform, camera, light, mesh, material, rigidBody, animator, billboard,
        /// particles, healthBar, billboardText, fullscreenEffect) or user:<typeId>.
        static bool CopyComponent(const Scene &scene, fr::Entity entity, std::string_view kind,
                                  std::string &outJson);

        /// Apply a component JSON blob produced by CopyComponent onto @p entity.
        static bool PasteComponent(Scene &scene, fr::Entity entity, std::string_view json);
    };

} // namespace FRIGGA_NAMESPACE
