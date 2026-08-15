#pragma once

#include "Frigga/ECS/Components/UserDataComponent.hpp"
#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Physics/PhysicsTypes.hpp"
#include "Frigga/Plugin/GameplayTypeIds.hpp"

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>

namespace FRIGGA_NAMESPACE
{

    [[nodiscard]] inline float PropertyFloat(const UserComponentInstance &instance,
                                             std::string_view name, float fallback)
    {
        const auto *property = FindProperty(instance, name);
        if(property == nullptr)
        {
            return fallback;
        }
        if(property->value.kind == PropertyKind::Float)
        {
            return property->value.floatValue;
        }
        if(property->value.kind == PropertyKind::Int64)
        {
            return static_cast<float>(property->value.intValue);
        }
        return fallback;
    }

    [[nodiscard]] inline glm::vec3 PropertyVec3(const UserComponentInstance &instance,
                                                std::string_view name, const glm::vec3 &fallback)
    {
        const auto *property = FindProperty(instance, name);
        if(property == nullptr || property->value.kind != PropertyKind::Vec3)
        {
            return fallback;
        }
        return property->value.vec3Value;
    }

    [[nodiscard]] inline std::int64_t PropertyInt(const UserComponentInstance &instance,
                                                  std::string_view name, std::int64_t fallback)
    {
        const auto *property = FindProperty(instance, name);
        if(property == nullptr)
        {
            return fallback;
        }
        if(property->value.kind == PropertyKind::Int64)
        {
            return property->value.intValue;
        }
        if(property->value.kind == PropertyKind::Float)
        {
            return static_cast<std::int64_t>(property->value.floatValue);
        }
        return fallback;
    }

    [[nodiscard]] inline PhysicsCharacterDesc CharacterDescFromInstance(
        const UserComponentInstance &instance)
    {
        PhysicsCharacterDesc desc {};
        desc.radius          = PropertyFloat(instance, "radius", 0.35f);
        desc.height          = PropertyFloat(instance, "height", 1.0f);
        desc.maxSlopeDegrees = PropertyFloat(instance, "maxSlopeDegrees", 45.0f);
        desc.mass            = PropertyFloat(instance, "mass", 70.0f);
        desc.centerOffset    = PropertyVec3(instance, "centerOffset", {});
        desc.collisionLayer  = static_cast<std::uint8_t>(
            std::clamp<std::int64_t>(PropertyInt(instance, "collisionLayer", 1), 0, 15));
        desc.collideWithLayers = static_cast<std::uint16_t>(std::clamp<std::int64_t>(
            PropertyInt(instance, "collideWithLayers", 0xffff), 0, 0xffff));
        return desc;
    }

    [[nodiscard]] inline glm::vec3 CapsuleCenterLocalFromDesc(const PhysicsCharacterDesc &desc)
    {
        const float radius    = std::max(desc.radius, 0.001f);
        const float halfHeight = std::max(0.5f * desc.height, 0.001f);
        return desc.centerOffset + glm::vec3 {0.0f, halfHeight + radius, 0.0f};
    }

    [[nodiscard]] inline bool EntityHasCharacterController(fr::Registry &registry,
                                                           const UserComponentRegistry &catalog,
                                                           fr::Entity entity)
    {
        const auto ops = catalog.Find(kCharacterControllerTypeId);
        return ops && ops->has && ops->has(registry, entity);
    }

} // namespace FRIGGA_NAMESPACE
