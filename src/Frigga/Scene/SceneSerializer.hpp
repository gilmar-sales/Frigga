#pragma once

#include "Frigga/Scene/Scene.hpp"

#include <filesystem>

namespace FRIGGA_NAMESPACE
{

    class SceneSerializer
    {
      public:
        static bool Save(Scene &scene, const std::filesystem::path &path);
        static bool Load(Scene &scene, const std::filesystem::path &path);
    };

} // namespace FRIGGA_NAMESPACE
