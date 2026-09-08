#pragma once

#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/UserDataComponent.hpp"
#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Physics/PhysicsTypes.hpp"
#include "Frigga/Module/GameplayTypeIds.hpp"

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

    /// Locomotion-only fields from CharacterControllerComponent (+ legacy shape props if present).
    [[nodiscard]] inline PhysicsCharacterDesc CharacterDescFromInstance(
        const UserComponentInstance &instance)
    {
        PhysicsCharacterDesc desc {};
        // Legacy shape fields — preferred source is RigidBodyComponent via ApplyRigidBodyToDesc.
        desc.radius          = PropertyFloat(instance, "radius", 0.5f);
        desc.height          = PropertyFloat(instance, "height", 1.0f);
        desc.maxSlopeDegrees = PropertyFloat(instance, "maxSlopeDegrees", 45.0f);
        desc.mass            = PropertyFloat(instance, "mass", 70.0f);
        desc.maxStrength     = PropertyFloat(instance, "maxStrength", 100.0f);
        desc.centerOffset    = PropertyVec3(instance, "centerOffset", {});
        desc.stickToFloorDistance =
            PropertyFloat(instance, "stickToFloorDistance", 0.5f);
        desc.walkStairsStepHeight =
            PropertyFloat(instance, "walkStairsStepHeight", 0.4f);
        desc.predictiveContactDistance =
            PropertyFloat(instance, "predictiveContactDistance", 0.1f);
        desc.characterPadding =
            PropertyFloat(instance, "characterPadding", 0.02f);
        desc.penetrationRecoverySpeed =
            PropertyFloat(instance, "penetrationRecoverySpeed", 1.0f);
        desc.enhancedInternalEdgeRemoval =
            PropertyInt(instance, "enhancedInternalEdgeRemoval", 0) != 0;
        desc.collisionLayer  = static_cast<std::uint8_t>(
            std::clamp<std::int64_t>(PropertyInt(instance, "collisionLayer", 1), 0, 15));
        desc.collideWithLayers = static_cast<std::uint16_t>(std::clamp<std::int64_t>(
            PropertyInt(instance, "collideWithLayers", 0xffff), 0, 0xffff));
        return desc;
    }

    /// Overlay capsule / mass / layers from the required RigidBody presence collider.
    inline void ApplyRigidBodyToCharacterDesc(PhysicsCharacterDesc &desc,
                                              const RigidBodyComponent &rigidBody)
    {
        if(rigidBody.shape == ColliderShape::Capsule || rigidBody.shape == ColliderShape::Sphere)
        {
            desc.radius = rigidBody.radius;
            if(rigidBody.shape == ColliderShape::Capsule)
            {
                desc.height = rigidBody.height;
            }
        }
        desc.centerOffset      = rigidBody.centerOffset;
        desc.mass              = rigidBody.mass > 0.0f ? rigidBody.mass : desc.mass;
        desc.collisionLayer    = rigidBody.collisionLayer;
        desc.collideWithLayers = rigidBody.collideWithLayers;
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

    [[nodiscard]] inline RigidBodyComponent MakeDefaultCharacterRigidBody()
    {
        RigidBodyComponent rb {};
        rb.motion            = BodyMotionType::Kinematic;
        rb.shape             = ColliderShape::Capsule;
        rb.radius            = 0.5f;
        rb.height            = 1.0f;
        rb.mass              = 70.0f;
        rb.collisionLayer    = 1;
        rb.collideWithLayers = 0xffff;
        return rb;
    }

} // namespace FRIGGA_NAMESPACE
