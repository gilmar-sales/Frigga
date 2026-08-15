#include "ThirdPersonCameraSystem.hpp"

#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/ThirdPersonCameraComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    ThirdPersonCameraSystem::ThirdPersonCameraSystem(const skr::Arc<fr::Registry> &registry,
                                                     const skr::Arc<Input> &input,
                                                     const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mInput(input), mSimulation(simulation)
    {
    }

    void ThirdPersonCameraSystem::Update(float)
    {
        if(!mSimulation || !mSimulation->IsPlaying() || !mSimulation->IsRunning())
        {
            return;
        }
        if(!mInput)
        {
            return;
        }

        std::unordered_map<std::string, glm::vec3> namedPositions;
        mRegistry->CreateMutation()->Each<NameComponent, TransformComponent>(
            [&](auto, NameComponent &name, TransformComponent &targetTransform) {
                namedPositions[name.name] = targetTransform.position;
            });

        mRegistry->CreateMutation()->Each<TransformComponent, ThirdPersonCameraComponent>(
            [&](auto, TransformComponent &transform, ThirdPersonCameraComponent &orbit) {
                orbit.yaw -= mInput->GetAxis(orbit.lookXAxis);
                orbit.pitch += mInput->GetAxis(orbit.lookYAxis);
                orbit.pitch =
                    std::clamp(orbit.pitch, orbit.minPitch, orbit.maxPitch);

                orbit.distance -= mInput->GetAxis(orbit.zoomAxis);
                orbit.distance =
                    std::clamp(orbit.distance, orbit.minDistance, orbit.maxDistance);

                glm::vec3 targetPos = transform.position;
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

                transform.position = pivot + offset;
                const glm::vec3 toPivot = pivot - transform.position;
                if(glm::dot(toPivot, toPivot) < 1e-8f)
                {
                    return;
                }
                transform.rotation = glm::quatLookAt(glm::normalize(toPivot), {0.0f, 1.0f, 0.0f});
            });
    }

} // namespace FRIGGA_NAMESPACE
