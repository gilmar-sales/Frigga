#pragma once

#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>

#include <vector>

namespace FRIGGA_NAMESPACE
{

    inline constexpr fr::Entity kInvalidEntity = static_cast<fr::Entity>(-1);

    /// Parent/child links. TransformComponent is always local; world = parentWorld * local.
    struct HierarchyComponent: fr::Component
    {
        fr::Entity              parent = kInvalidEntity;
        std::vector<fr::Entity> children;
    };

} // namespace FRIGGA_NAMESPACE
