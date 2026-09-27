#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FRIGGA_NAMESPACE {

/// Local pose (relative to the fr::ChildOf parent). Propagated into WorldTransformComponent by
/// TransformPolicy; call registry.MarkHierarchyDirty<TransformComponent>(entity) after edits.
struct TransformComponent: fr::HierarchyLocal
{
    glm::vec3 position{};
    glm::vec3 scale{1, 1, 1};
    glm::quat rotation{1, 0, 0, 0};
};

}
