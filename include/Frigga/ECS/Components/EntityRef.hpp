#pragma once

#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Distinct from fr::Entity (uint32_t) so user-component reflection can treat it as
    /// PropertyKind::Entity instead of Int64. Runtime stores a raw Freyr id (no generation).
    struct EntityRef
    {
        fr::Entity id = kInvalidEntity;
    };

    /// ImGui drag-drop payload when dragging an entity from the Hierarchy panel.
    inline constexpr const char *kHierarchyEntityDragPayload = "FRIGGA_HIERARCHY_ENTITY";

} // namespace FRIGGA_NAMESPACE
