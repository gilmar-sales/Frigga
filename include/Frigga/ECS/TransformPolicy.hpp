#pragma once

#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/Components/WorldTransformComponent.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    /// fr::HierarchyPropagationPolicy for Frigga transforms: world = parentWorld * local.
    /// Tolerates hierarchy nodes without transforms (identity parent, child skipped) so a
    /// missing WorldTransformComponent never faults the propagation.
    struct TransformPolicy
    {
        using Local = TransformComponent;
        using World = WorldTransformComponent;

        void OnRoot(fr::ComponentManager &components, fr::Entity entity) const;
        void Propagate(fr::ComponentManager &components, fr::Entity parent,
                       fr::Entity child) const;

        bool HasChildrenInterest(fr::ComponentManager &, fr::Entity) const
        {
            return true;
        }
    };

    static_assert(fr::HierarchyPropagationPolicy<TransformPolicy>);

} // namespace FRIGGA_NAMESPACE
