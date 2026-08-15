#include "systems/ThirdPersonCameraSystem.hpp"

#include "components/ThirdPersonCameraComponent.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

ThirdPersonCameraSystem::ThirdPersonCameraSystem(const skr::Arc<fr::Registry> &registry,
                                                 const skr::Arc<fg::Input> &input)
    : fr::System(registry), mInput(input)
{
}

void ThirdPersonCameraSystem::Update(float)
{
    if(!mInput)
    {
        return;
    }

    std::unordered_map<std::string, glm::vec3> namedPositions;
    mRegistry->CreateMutation()->Each<fg::NameComponent, fg::TransformComponent>(
        [&](auto entity, fg::NameComponent &name, fg::TransformComponent &) {
            namedPositions[name.name] = fg::TransformUtil::WorldPose(*mRegistry, entity).position;
        });

    mRegistry->CreateMutation()->Each<fg::TransformComponent, ThirdPersonCameraComponent>(
        [&](auto entity, fg::TransformComponent &, ThirdPersonCameraComponent &orbit) {
            orbit.yaw -= mInput->GetAxis(orbit.lookXAxis);
            orbit.pitch += mInput->GetAxis(orbit.lookYAxis);
            orbit.pitch = std::clamp(orbit.pitch, orbit.minPitch, orbit.maxPitch);

            orbit.distance -= mInput->GetAxis(orbit.zoomAxis);
            orbit.distance = std::clamp(orbit.distance, orbit.minDistance, orbit.maxDistance);

            glm::vec3 targetPos = fg::TransformUtil::WorldPose(*mRegistry, entity).position;
            if(const auto found = namedPositions.find(orbit.targetName);
               found != namedPositions.end())
            {
                targetPos = found->second;
            }

            const glm::vec3 pivot = targetPos + orbit.pivotOffset;
            const float yawRad    = glm::radians(orbit.yaw);
            const float pitchRad  = glm::radians(orbit.pitch);
            const float cosPitch  = std::cos(pitchRad);

            const glm::vec3 offset {orbit.distance * cosPitch * std::sin(yawRad),
                                    orbit.distance * std::sin(pitchRad),
                                    orbit.distance * cosPitch * std::cos(yawRad)};

            const glm::vec3 worldPos = pivot + offset;
            const glm::vec3 toPivot  = pivot - worldPos;
            if(glm::dot(toPivot, toPivot) < 1e-8f)
            {
                fg::TransformUtil::SetWorldPose(*mRegistry, entity, worldPos,
                                                glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                return;
            }
            fg::TransformUtil::SetWorldPose(*mRegistry, entity, worldPos,
                                            glm::quatLookAt(glm::normalize(toPivot),
                                                            {0.0f, 1.0f, 0.0f}));
        });
}
