#pragma once

#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/Components/WorldTransformComponent.hpp"

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

/// Hierarchy is Freyr's native one (fr::ChildOf via registry.SetParent / ClearParent /
/// GetParent / Children). WorldTransformComponent is propagated by TransformPolicy in
/// PostUpdate; these helpers cover world-space reads/writes and reparenting.
namespace FRIGGA_NAMESPACE::TransformUtil
{

    struct Pose
    {
        glm::vec3 position {};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale {1.0f, 1.0f, 1.0f};
    };

    [[nodiscard]] glm::mat4 LocalMatrix(const TransformComponent &transform);
    [[nodiscard]] Pose      Decompose(const glm::mat4 &matrix);
    void                    ApplyLocalMatrix(TransformComponent &transform, const glm::mat4 &matrix);

    /// registry.MarkHierarchyDirty<TransformComponent>. Not thread-safe: entities written from
    /// EachAsync / ForEachChunkAsync must be marked after the parallel pass.
    void MarkDirty(fr::Registry &registry, fr::Entity entity);

    /// Parent's propagated world (identity for roots or parents without transform).
    [[nodiscard]] glm::mat4 ParentWorldMatrix(fr::Registry &registry, fr::Entity entity);

    /// Parent's propagated world times the entity's current local. Fresh after edits to the
    /// entity itself since the last propagation; ancestors edited this frame show up after
    /// PostUpdate (or RefreshWorld).
    [[nodiscard]] glm::mat4 GetWorldMatrix(fr::Registry &registry, fr::Entity entity);
    [[nodiscard]] Pose      GetWorldPose(fr::Registry &registry, fr::Entity entity);

    /// Local that lands on `world` under `parentWorld` (inverse(parentWorld) * world).
    [[nodiscard]] glm::mat4 LocalFromWorld(const glm::mat4 &parentWorld, const glm::mat4 &world);

    /// World-space writes: rewrite the local from the parent world, refresh the entity subtree
    /// and mark the hierarchy dirty.
    void SetWorldMatrix(fr::Registry &registry, fr::Entity entity, const glm::mat4 &world);
    void SetWorldPose(fr::Registry &registry, fr::Entity entity, const glm::vec3 &position,
                      const glm::quat &rotation);
    /// Writes world position only; preserves world rotation and scale.
    void SetWorldPosition(fr::Registry &registry, fr::Entity entity, const glm::vec3 &position);

    /// Recomputes WorldTransformComponent for `entity` and its descendants right away (editor
    /// operations and loaders that read world poses before the next PostUpdate).
    void RefreshWorld(fr::Registry &registry, fr::Entity entity);
    /// RefreshWorld for every root carrying a transform.
    void RefreshAllWorlds(fr::Registry &registry);

    /// registry.SetParent (ClearParent when parent == fr::NullEntity), keeping the world pose
    /// when requested. False for self/cycle/invalid parents.
    bool Reparent(fr::Registry &registry, fr::Entity entity, fr::Entity parent,
                  bool preserveWorld = true);

    /// Adds WorldTransformComponent to every new TransformComponent and marks it dirty so
    /// propagation picks it up. Installed once per registry (Scene does it).
    void InstallObservers(fr::Registry &registry);

} // namespace FRIGGA_NAMESPACE::TransformUtil
