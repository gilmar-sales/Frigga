#include "Frigga/ECS/TransformUtil.hpp"

#include <vector>

namespace FRIGGA_NAMESPACE::TransformUtil
{
    namespace
    {
        /// Registry resource marking InstallObservers as done.
        struct TransformObserversInstalled
        {
        };

        void StoreWorld(fr::Registry &registry, fr::Entity entity, const glm::mat4 &parentWorld)
        {
            registry.TryGetComponents<TransformComponent, WorldTransformComponent>(
                entity, [&](TransformComponent &local, WorldTransformComponent &world) {
                    world.matrix = parentWorld * LocalMatrix(local);
                });
        }
    } // namespace

    glm::mat4 LocalMatrix(const TransformComponent &transform)
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
        model           = model * glm::mat4_cast(transform.rotation);
        model           = glm::scale(model, transform.scale);
        return model;
    }

    Pose Decompose(const glm::mat4 &matrix)
    {
        Pose pose {};
        pose.position = glm::vec3(matrix[3]);

        glm::vec3 col0(matrix[0]);
        glm::vec3 col1(matrix[1]);
        glm::vec3 col2(matrix[2]);
        pose.scale = {glm::length(col0), glm::length(col1), glm::length(col2)};

        if(pose.scale.x > 1e-8f)
        {
            col0 /= pose.scale.x;
        }
        else
        {
            col0 = {1.0f, 0.0f, 0.0f};
        }
        if(pose.scale.y > 1e-8f)
        {
            col1 /= pose.scale.y;
        }
        else
        {
            col1 = {0.0f, 1.0f, 0.0f};
        }
        if(pose.scale.z > 1e-8f)
        {
            col2 /= pose.scale.z;
        }
        else
        {
            col2 = {0.0f, 0.0f, 1.0f};
        }

        glm::mat3 rotation(col0, col1, col2);
        if(glm::determinant(rotation) < 0.0f)
        {
            pose.scale.x = -pose.scale.x;
            rotation[0]  = -rotation[0];
        }
        pose.rotation = glm::normalize(glm::quat_cast(rotation));
        return pose;
    }

    void ApplyLocalMatrix(TransformComponent &transform, const glm::mat4 &matrix)
    {
        const auto pose    = Decompose(matrix);
        transform.position = pose.position;
        transform.rotation = pose.rotation;
        transform.scale    = pose.scale;
    }

    void MarkDirty(fr::Registry &registry, fr::Entity entity)
    {
        registry.MarkHierarchyDirty<TransformComponent>(entity);
    }

    glm::mat4 ParentWorldMatrix(fr::Registry &registry, fr::Entity entity)
    {
        glm::mat4 parentWorld(1.0f);
        const auto parent = registry.GetParent(entity);
        if(parent != fr::NullEntity)
        {
            registry.TryGetComponents<WorldTransformComponent>(
                parent, [&](WorldTransformComponent &world) { parentWorld = world.matrix; });
        }
        return parentWorld;
    }

    glm::mat4 GetWorldMatrix(fr::Registry &registry, fr::Entity entity)
    {
        glm::mat4 local(1.0f);
        registry.TryGetComponents<TransformComponent>(
            entity, [&](TransformComponent &transform) { local = LocalMatrix(transform); });
        return ParentWorldMatrix(registry, entity) * local;
    }

    Pose GetWorldPose(fr::Registry &registry, fr::Entity entity)
    {
        return Decompose(GetWorldMatrix(registry, entity));
    }

    glm::mat4 LocalFromWorld(const glm::mat4 &parentWorld, const glm::mat4 &world)
    {
        return glm::inverse(parentWorld) * world;
    }

    void SetWorldMatrix(fr::Registry &registry, fr::Entity entity, const glm::mat4 &world)
    {
        const glm::mat4 local = LocalFromWorld(ParentWorldMatrix(registry, entity), world);
        const bool      written = registry.TryGetComponents<TransformComponent>(
            entity, [&](TransformComponent &transform) { ApplyLocalMatrix(transform, local); });
        if(!written)
        {
            return;
        }
        RefreshWorld(registry, entity);
        MarkDirty(registry, entity);
    }

    void SetWorldPose(fr::Registry &registry, fr::Entity entity, const glm::vec3 &position,
                      const glm::quat &rotation)
    {
        const auto current = GetWorldPose(registry, entity);
        glm::mat4 world    = glm::translate(glm::mat4(1.0f), position);
        world              = world * glm::mat4_cast(rotation);
        world              = glm::scale(world, current.scale);
        SetWorldMatrix(registry, entity, world);
    }

    void SetWorldPosition(fr::Registry &registry, fr::Entity entity, const glm::vec3 &position)
    {
        const auto current = GetWorldPose(registry, entity);
        SetWorldPose(registry, entity, position, current.rotation);
    }

    void RefreshWorld(fr::Registry &registry, fr::Entity entity)
    {
        StoreWorld(registry, entity, ParentWorldMatrix(registry, entity));
        // Pre-order: each parent is stored before its children read it.
        registry.ForEachDescendant(entity, [&](fr::Entity child) {
            StoreWorld(registry, child, ParentWorldMatrix(registry, child));
        });
    }

    void RefreshAllWorlds(fr::Registry &registry)
    {
        std::vector<fr::Entity> roots;
        registry.CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, WorldTransformComponent &) {
                if(registry.GetParent(entity) == fr::NullEntity)
                {
                    roots.push_back(entity);
                }
            });
        for(const auto root : roots)
        {
            RefreshWorld(registry, root);
        }
    }

    bool Reparent(fr::Registry &registry, fr::Entity entity, fr::Entity parent,
                  bool preserveWorld)
    {
        if(entity == fr::NullEntity || entity == parent || !registry.IsAlive(entity))
        {
            return false;
        }
        if(parent != fr::NullEntity && !registry.IsAlive(parent))
        {
            return false;
        }

        const bool      hasTransform = registry.HasComponent<TransformComponent>(entity);
        const glm::mat4 world = hasTransform ? GetWorldMatrix(registry, entity) : glm::mat4(1.0f);

        const bool linked = parent == fr::NullEntity ? registry.ClearParent(entity)
                                                     : registry.SetParent(entity, parent);
        if(!linked)
        {
            return false;
        }

        if(hasTransform)
        {
            if(preserveWorld)
            {
                const glm::mat4 parentWorld =
                    parent == fr::NullEntity ? glm::mat4(1.0f) : GetWorldMatrix(registry, parent);
                registry.TryGetComponents<TransformComponent>(
                    entity, [&](TransformComponent &transform) {
                        ApplyLocalMatrix(transform, LocalFromWorld(parentWorld, world));
                    });
            }
            RefreshWorld(registry, entity);
            MarkDirty(registry, entity);
        }
        return true;
    }

    void InstallObservers(fr::Registry &registry)
    {
        if(registry.HasResource<TransformObserversInstalled>())
        {
            return;
        }
        registry.InsertResource(TransformObserversInstalled {});

        auto *target = &registry;
        registry.ObserveAdd<TransformComponent>([target](fr::Entity entity) {
            if(!target->IsAlive(entity) || !target->HasComponent<TransformComponent>(entity))
            {
                return;
            }
            if(!target->HasComponent<WorldTransformComponent>(entity))
            {
                target->AddComponents(
                    entity, WorldTransformComponent {.matrix = GetWorldMatrix(*target, entity)});
            }
            MarkDirty(*target, entity);
        });
    }

} // namespace FRIGGA_NAMESPACE::TransformUtil
