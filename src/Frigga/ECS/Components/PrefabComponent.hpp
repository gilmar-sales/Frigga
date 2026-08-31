#pragma once

#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>

#include <string>

namespace FRIGGA_NAMESPACE
{

    /// Marks an entity as an instance of a prefab asset under Resources/.
    struct PrefabComponent: fr::Component
    {
        /// Path relative to Resources/ (e.g. "Prefabs/Enemy.prefab").
        std::string source;
    };

} // namespace FRIGGA_NAMESPACE
