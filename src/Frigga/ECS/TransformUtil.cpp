#include "Frigga/ECS/TransformUtil.hpp"

#include "Frigga/ECS/Components/NameComponent.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace FRIGGA_NAMESPACE::TransformUtil
{
    namespace
    {
        glm::mat4 ParentWorldMatrix(fr::Registry &registry, fr::Entity entity)
        {
            const auto parent = ParentOf(registry, entity);
            if(parent == kInvalidEntity)
            {
                return glm::mat4(1.0f);
            }
            return WorldMatrix(registry, parent);
        }

        void EnsureHierarchy(fr::Registry &registry, fr::Entity entity)
        {
            if(entity == kInvalidEntity)
            {
                return;
            }
            if(!registry.HasComponent<HierarchyComponent>(entity))
            {
                registry.AddComponents(entity, HierarchyComponent {});
                registry.ExecuteTasks();
            }
        }

        void DetachFromParent(fr::Registry &registry, fr::Entity entity)
        {
            const auto oldParent = ParentOf(registry, entity);
            if(oldParent == kInvalidEntity)
            {
                return;
            }
            registry.TryGetComponents<HierarchyComponent>(oldParent, [&](HierarchyComponent &hierarchy) {
                std::erase(hierarchy.children, entity);
            });
        }

        void CollectSubtree(fr::Registry &registry, fr::Entity entity, std::vector<fr::Entity> &out,
                            std::unordered_set<fr::Entity> &seen)
        {
            if(entity == kInvalidEntity || !seen.insert(entity).second)
            {
                return;
            }
            std::vector<fr::Entity> children;
            registry.TryGetComponents<HierarchyComponent>(entity, [&](HierarchyComponent &hierarchy) {
                children = hierarchy.children;
            });
            for(const auto child : children)
            {
                CollectSubtree(registry, child, out, seen);
            }
            out.push_back(entity);
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
        const auto pose  = Decompose(matrix);
        transform.position = pose.position;
        transform.rotation = pose.rotation;
        transform.scale    = pose.scale;
    }

    fr::Entity ParentOf(fr::Registry &registry, fr::Entity entity)
    {
        fr::Entity parent = kInvalidEntity;
        if(entity == kInvalidEntity)
        {
            return parent;
        }
        registry.TryGetComponents<HierarchyComponent>(entity, [&](HierarchyComponent &hierarchy) {
            parent = hierarchy.parent;
        });
        if(parent == kInvalidEntity)
        {
            return kInvalidEntity;
        }
        if(!registry.HasComponent<HierarchyComponent>(parent) &&
           !registry.HasComponent<TransformComponent>(parent) &&
           !registry.HasComponent<NameComponent>(parent))
        {
            return kInvalidEntity;
        }
        return parent;
    }

    bool WouldCreateCycle(fr::Registry &registry, fr::Entity entity, fr::Entity newParent)
    {
        if(entity == kInvalidEntity || newParent == kInvalidEntity)
        {
            return false;
        }
        if(entity == newParent)
        {
            return true;
        }
        std::unordered_set<fr::Entity> seen;
        auto current = newParent;
        while(current != kInvalidEntity)
        {
            if(current == entity)
            {
                return true;
            }
            if(!seen.insert(current).second)
            {
                return true;
            }
            current = ParentOf(registry, current);
        }
        return false;
    }

    glm::mat4 WorldMatrix(fr::Registry &registry, fr::Entity entity)
    {
        std::vector<fr::Entity> chain;
        std::unordered_set<fr::Entity> seen;
        auto current = entity;
        while(current != kInvalidEntity)
        {
            if(!seen.insert(current).second)
            {
                break;
            }
            chain.push_back(current);
            current = ParentOf(registry, current);
        }

        glm::mat4 world(1.0f);
        for(auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            registry.TryGetComponents<TransformComponent>(*it, [&](TransformComponent &transform) {
                world = world * LocalMatrix(transform);
            });
        }
        return world;
    }

    Pose WorldPose(fr::Registry &registry, fr::Entity entity)
    {
        return Decompose(WorldMatrix(registry, entity));
    }

    void SetWorldMatrix(fr::Registry &registry, fr::Entity entity, const glm::mat4 &world)
    {
        if(!registry.HasComponent<TransformComponent>(entity))
        {
            return;
        }
        const glm::mat4 parentWorld = ParentWorldMatrix(registry, entity);
        const glm::mat4 local       = glm::inverse(parentWorld) * world;
        registry.TryGetComponents<TransformComponent>(entity, [&](TransformComponent &transform) {
            ApplyLocalMatrix(transform, local);
        });
    }

    void SetWorldPose(fr::Registry &registry, fr::Entity entity, const glm::vec3 &position,
                      const glm::quat &rotation)
    {
        const auto current = WorldPose(registry, entity);
        glm::mat4 world    = glm::translate(glm::mat4(1.0f), position);
        world              = world * glm::mat4_cast(rotation);
        world              = glm::scale(world, current.scale);
        SetWorldMatrix(registry, entity, world);
    }

    bool SetParent(fr::Registry &registry, fr::Entity entity, fr::Entity newParent,
                   bool preserveWorld)
    {
        if(entity == kInvalidEntity || entity == newParent)
        {
            return false;
        }
        if(newParent != kInvalidEntity && WouldCreateCycle(registry, entity, newParent))
        {
            return false;
        }

        glm::mat4 world(1.0f);
        const bool captureWorld =
            preserveWorld && registry.HasComponent<TransformComponent>(entity);
        if(captureWorld)
        {
            world = WorldMatrix(registry, entity);
        }

        DetachFromParent(registry, entity);
        EnsureHierarchy(registry, entity);
        registry.TryGetComponents<HierarchyComponent>(entity, [&](HierarchyComponent &hierarchy) {
            hierarchy.parent = newParent;
        });

        if(newParent != kInvalidEntity)
        {
            EnsureHierarchy(registry, newParent);
            registry.TryGetComponents<HierarchyComponent>(
                newParent, [&](HierarchyComponent &hierarchy) {
                    if(std::ranges::find(hierarchy.children, entity) == hierarchy.children.end())
                    {
                        hierarchy.children.push_back(entity);
                    }
                });
        }

        if(captureWorld)
        {
            SetWorldMatrix(registry, entity, world);
        }
        return true;
    }

    void DestroySubtree(fr::Registry &registry, fr::Entity entity)
    {
        std::vector<fr::Entity> order;
        std::unordered_set<fr::Entity> seen;
        CollectSubtree(registry, entity, order, seen);
        for(const auto node : order)
        {
            DetachFromParent(registry, node);
            registry.DestroyEntity(node);
        }
        registry.ExecuteTasks();
    }

} // namespace FRIGGA_NAMESPACE::TransformUtil
