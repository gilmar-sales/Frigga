#pragma once

#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

    [[nodiscard]] fr::Entity ParentOf(fr::Registry &registry, fr::Entity entity);
    [[nodiscard]] bool       WouldCreateCycle(fr::Registry &registry, fr::Entity entity,
                                              fr::Entity newParent);

    [[nodiscard]] glm::mat4 WorldMatrix(fr::Registry &registry, fr::Entity entity);
    [[nodiscard]] Pose      WorldPose(fr::Registry &registry, fr::Entity entity);

    void SetWorldMatrix(fr::Registry &registry, fr::Entity entity, const glm::mat4 &world);
    void SetWorldPose(fr::Registry &registry, fr::Entity entity, const glm::vec3 &position,
                      const glm::quat &rotation);

    bool SetParent(fr::Registry &registry, fr::Entity entity, fr::Entity newParent,
                   bool preserveWorld = true);
    void DestroySubtree(fr::Registry &registry, fr::Entity entity);

} // namespace FRIGGA_NAMESPACE::TransformUtil
