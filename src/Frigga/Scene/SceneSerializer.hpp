#pragma once

#include "Frigga/Scene/Scene.hpp"

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
    };

} // namespace FRIGGA_NAMESPACE
