#include "systems/CharacterMovementSystem.hpp"

#include "components/CharacterControllerComponent.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <glm/glm.hpp>
#include <cmath>

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
    const float speed      = 4.0f;
    const float jumpSpeed  = 5.0f;

    glm::quat cameraRotation {1.0f, 0.0f, 0.0f, 0.0f};
    bool      hasCamera = false;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, fg::TransformComponent &) {
            if(name.name != "Main Camera")
            {
                return;
            }
            cameraRotation = fg::TransformUtil::WorldPose(*mRegistry, entity).rotation;
            hasCamera      = true;
        });

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, CharacterControllerComponent &) {
            if(name.name != "Player")
            {
                return;
            }

            glm::vec3 desired;
            if(hasCamera)
            {
                const glm::vec3 forward = cameraRotation * glm::vec3 {0.0f, 0.0f, -1.0f};
                glm::vec3 flatForward {forward.x, 0.0f, forward.z};
                if(glm::dot(flatForward, flatForward) < 1e-8f)
                {
                    desired = {horizontal * speed, 0.0f, -vertical * speed};
                }
                else
                {
                    flatForward             = glm::normalize(flatForward);
                    const glm::vec3 right =
                        glm::normalize(glm::cross(flatForward, glm::vec3 {0.0f, 1.0f, 0.0f}));
                    desired = (right * horizontal + flatForward * vertical) * speed;
                }
            }
            else
            {
                desired = {horizontal * speed, 0.0f, -vertical * speed};
            }
            const bool grounded = mPhysics->IsCharacterGrounded(entity);
            if(jump && grounded)
            {
                desired.y = jumpSpeed;
            }
            else if(!grounded)
            {
                desired.y = mPhysics->GetCharacterVelocity(entity).y;
            }
            mPhysics->MoveCharacter(entity, desired);
        });
}
