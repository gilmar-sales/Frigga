#include "systems/CharacterMovementSystem.hpp"

#include "components/CharacterControllerComponent.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
    constexpr glm::vec3 WorldUp{0.0f, 1.0f, 0.0f};
    constexpr glm::vec3 WorldForward{0.0f, 0.0f, -1.0f};

    glm::vec3 CalculatePlanarMovement(float horizontal, float vertical, float speed,
                                      const glm::quat &cameraRotation, bool hasCamera)
    {
        glm::vec3 direction;
        if(hasCamera)
        {
            const glm::vec3 forward = cameraRotation * WorldForward;
            glm::vec3 flatForward{forward.x, 0.0f, forward.z};
            if(glm::dot(flatForward, flatForward) >= 1e-8f)
            {
                flatForward           = glm::normalize(flatForward);
                const glm::vec3 right = glm::normalize(glm::cross(flatForward, WorldUp));
                direction             = right * horizontal + flatForward * vertical;
            }
            else
            {
                direction = {horizontal, 0.0f, -vertical};
            }
        }
        else
        {
            direction = {horizontal, 0.0f, -vertical};
        }

        const float lengthSquared = glm::dot(direction, direction);
        if(lengthSquared > 1.0f)
        {
            direction *= glm::inversesqrt(lengthSquared);
        }
        return direction * speed;
    }
} // namespace

CharacterMovementSystem::CharacterMovementSystem(const skr::Arc<fr::Registry> &registry,
                                                 const skr::Arc<fg::Input> &input,
                                                 const skr::Arc<fg::Physics> &physics)
    : fr::System(registry), mInput(input), mPhysics(physics)
{
}

void CharacterMovementSystem::Update(float)
{
    if(!mInput || !mPhysics)
    {
        return;
    }

    const float horizontal = mInput->GetAxis("Horizontal");
    const float vertical   = mInput->GetAxis("Vertical");
    const bool jump        = mInput->WasPressed("Jump");
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
    bool hasCamera = false;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, fg::TransformComponent &) {
            if(name.name != "Main Camera")
            {
                return;
            }
            cameraRotation = fg::TransformUtil::GetWorldPose(*mRegistry, entity).rotation;
            hasCamera      = true;
        });

    mRegistry->CreateMutation()->Each([&](fr::Entity entity, fg::NameComponent &name,
                                          CharacterControllerComponent &controller,
                                          fg::RigidBodyComponent &) {
        if(name.name != "Player")
        {
            return;
        }
        if(controller.locomotionLocked)
        {
            return;
        }

        glm::vec3 desired = CalculatePlanarMovement(horizontal, vertical, controller.movementSpeed,
                                                    cameraRotation, hasCamera);
        const bool grounded = mPhysics->IsCharacterGrounded(entity);
        if(grounded)
        {
            const auto ground = mPhysics->GetCharacterGroundInfo(entity);
            desired.x += ground.velocity.x;
            desired.z += ground.velocity.z;
        }
        // Only touch Y on jump — leave gravity / fall velocity to the Dynamic body.
        desired.y = mPhysics->GetCharacterVelocity(entity).y;
        if(jump && grounded)
        {
            desired.y = controller.jumpSpeed;
        }

        const glm::vec3 planar{desired.x, 0.0f, desired.z};
        if(glm::dot(planar, planar) > 1e-6f)
        {
            const glm::vec3 dir    = glm::normalize(planar);
            const glm::quat facing = glm::quatLookAt(dir, glm::vec3{0.0f, 1.0f, 0.0f});
            mPhysics->SetCharacterFacing(entity, facing);
        }

        mPhysics->MoveCharacter(entity, desired);
    });
}
