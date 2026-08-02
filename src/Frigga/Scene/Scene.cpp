#include "Scene.hpp"

#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        TransformComponent makeLookAtTransform(const glm::vec3 &position, const glm::vec3 &target,
                                               const glm::vec3 &worldUp = {0.0f, 1.0f, 0.0f})
        {
            const glm::vec3 forward = glm::normalize(target - position);
            // Match Freya/OpenGL: camera looks along local -Z.
            return TransformComponent {
                .position = position,
                .scale    = {1.0f, 1.0f, 1.0f},
                .rotation = glm::quatLookAt(forward, worldUp),
            };
        }
    } // namespace

    Scene::Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
                 const skr::Arc<fr::Registry> &ecsRegistry,
                 const skr::Arc<fra::LightService> &lightService,
                 const skr::Arc<PrimitiveMeshFactory> &primitives)
        : mEcsRegistry(ecsRegistry), mRenderer(renderer), mLogger(logger),
          mLightService(lightService), mPrimitives(primitives)
    {
        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Cube"}, TransformComponent {},
            MeshComponent {.meshId     = mPrimitives->GetMesh(PrimitiveType::Cube),
                           .materialId = mPrimitives->GetDefaultMaterial()});

        // Blender-like default framing: elevated 3/4 view looking at the unit cube.
        constexpr glm::vec3 cubeCenter {0.0f, 0.0f, 0.0f};
        constexpr glm::vec3 cameraPosition {4.0f, 3.0f, 4.0f};

        mMainCameraEntity = mEcsRegistry->CreateEntity(
            NameComponent {.name = "Main Camera"}, makeLookAtTransform(cameraPosition, cubeCenter),
            CameraComponent {.fovDegrees = 50.0f,
                             .nearPlane  = 0.1f,
                             .farPlane   = 1000.0f,
                             .primary    = true,
                             .locked     = true});

        mLightService->AddLight(fra::Light {
            .position  = {4.0f, 6.0f, 2.0f},
            .type      = static_cast<float>(fra::LightType::Point),
            .color     = {1.0f, 1.0f, 1.0f},
            .radius    = 40.0f,
            .direction = {0.0f, -1.0f, 0.0f},
            .intensity = 30.0f,
        });
    }

    void Scene::Update(float ts)
    {
        mLogger->LogTrace("scene update");
    }

    void Scene::OnEditorRender(float ts) {}

} // namespace FRIGGA_NAMESPACE
