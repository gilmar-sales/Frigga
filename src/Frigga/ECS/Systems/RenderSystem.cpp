#include "RenderSystem.hpp"

#include "../Components/CameraComponent.hpp"
#include "../Components/LightComponent.hpp"
#include "../Components/MaterialComponent.hpp"
#include "../Components/MeshComponent.hpp"
#include "../Components/TransformComponent.hpp"
#include "Frigga/Scene/Scene.hpp"

#include <algorithm>
#include <cmath>

namespace FRIGGA_NAMESPACE
{

    RenderSystem::RenderSystem(const skr::Arc<fr::Registry> &registry,
                               const skr::Arc<fra::Renderer> &renderer,
                               const skr::Arc<fra::Window> &window,
                               const skr::Arc<fra::LightService> &lightService,
                               const skr::Arc<Scene> &scene)
        : System(registry), mRenderer(renderer), mWindow(window), mLightService(lightService),
          mScene(scene)
    {
    }

    void RenderSystem::Update(float deltaTime)
    {
        updateCamera();
        syncLights();
        drawMeshes();
    }

    void RenderSystem::applyCameraPose(const TransformComponent &transform, float fovDegrees,
                                       float nearPlane, float farPlane)
    {
        float aspect = 16.0f / 9.0f;
        if(const auto target = mRenderer->GetOutputTarget())
        {
            const auto extent = target->GetExtent();
            if(extent.width > 0 && extent.height > 0)
            {
                aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
            }
        }
        else if(mWindow->GetWidth() > 0 && mWindow->GetHeight() > 0)
        {
            aspect = static_cast<float>(mWindow->GetWidth()) /
                     static_cast<float>(mWindow->GetHeight());
        }

        auto projection       = mRenderer->GetCurrentProjection();
        projection.projection =
            mRenderer->MakeProjection(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
        if(projection.ambientLight.w <= 0.0f)
        {
            projection.ambientLight = glm::vec4(0.0f, 1.0f, 0.0f, 0.35f);
        }
        mRenderer->UpdateProjection(projection);

        // Freya/OpenGL convention: camera looks along local -Z.
        const glm::vec3 forward =
            glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 up = glm::normalize(transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        if(glm::dot(forward, forward) < 1e-6f || glm::dot(up, up) < 1e-6f)
        {
            return;
        }

        mRenderer->UpdateCamera(transform.position, transform.position + forward, up);
    }

    void RenderSystem::updateCamera()
    {
        if(mScene->IsUsingEditorCamera())
        {
            const auto &editorCamera = mScene->GetEditorCamera();
            applyCameraPose(editorCamera.transform, editorCamera.fovDegrees,
                            editorCamera.nearPlane, editorCamera.farPlane);
            return;
        }

        bool updated = false;

        auto applyCamera = [this, &updated](TransformComponent &transform, CameraComponent &camera) {
            if(updated)
            {
                return;
            }

            applyCameraPose(transform, camera.fovDegrees, camera.nearPlane, camera.farPlane);
            updated = true;
        };

        // Prefer an explicitly marked primary camera.
        mRegistry->CreateMutation()->Each<TransformComponent, CameraComponent>(
            [&applyCamera](auto, TransformComponent &transform, CameraComponent &camera) {
                if(camera.primary)
                {
                    applyCamera(transform, camera);
                }
            });

        // Fallback: locked Main Camera, then any camera.
        if(!updated)
        {
            mRegistry->CreateMutation()->Each<TransformComponent, CameraComponent>(
                [&applyCamera](auto, TransformComponent &transform, CameraComponent &camera) {
                    if(camera.locked)
                    {
                        applyCamera(transform, camera);
                    }
                });
        }

        if(!updated)
        {
            mRegistry->CreateMutation()->Each<TransformComponent, CameraComponent>(
                [&applyCamera](auto, TransformComponent &transform, CameraComponent &camera) {
                    applyCamera(transform, camera);
                });
        }
    }

    void RenderSystem::syncLights()
    {
        mLightService->ClearLights();

        mRegistry->CreateMutation()->Each<TransformComponent, LightComponent>(
            [this](auto, TransformComponent &transform, LightComponent &light) {
                // Match Freya/OpenGL: entity local -Z is the aimed light direction.
                const glm::vec3 direction =
                    glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
                const glm::vec3 safeDirection =
                    glm::dot(direction, direction) > 1e-6f ? direction
                                                           : glm::vec3(0.0f, -1.0f, 0.0f);

                fra::Light gpuLight {};
                gpuLight.position  = transform.position;
                gpuLight.type      = static_cast<float>(light.type);
                gpuLight.color     = light.color;
                gpuLight.radius    = light.radius;
                gpuLight.direction = safeDirection;
                gpuLight.intensity = light.intensity;
                gpuLight.innerCutoff =
                    std::cos(glm::radians(std::max(light.innerAngleDegrees, 0.0f)));
                gpuLight.outerCutoff =
                    std::cos(glm::radians(std::max(light.outerAngleDegrees, 0.0f)));

                mLightService->AddLight(gpuLight);
            });
    }

    void RenderSystem::drawMeshes()
    {
        mInstanceMatrices.clear();

        struct DrawItem
        {
            std::uint32_t meshId;
            std::uint32_t materialId;
            std::uint32_t firstInstance;
            std::uint32_t entityId;
        };
        std::vector<DrawItem> draws;

        mRegistry->CreateMutation()->Each<TransformComponent, MeshComponent, MaterialComponent>(
            [this, &draws](auto entity, TransformComponent &transform, MeshComponent &mesh,
                           MaterialComponent &material) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
                model           = model * glm::mat4_cast(transform.rotation);
                model           = glm::scale(model, transform.scale);

                const auto firstInstance =
                    static_cast<std::uint32_t>(mInstanceMatrices.size());
                mInstanceMatrices.push_back(model);
                draws.push_back({mesh.meshId, material.materialId, firstInstance,
                                 static_cast<std::uint32_t>(entity)});
            });

        if(draws.empty())
        {
            return;
        }

        const auto requiredSize = sizeof(glm::mat4) * mInstanceMatrices.size();
        if(!mInstanceBuffer || mInstanceCapacity < mInstanceMatrices.size())
        {
            mInstanceCapacity = std::max<std::uint64_t>(mInstanceMatrices.size(), 16);
            mInstanceBuffer =
                mRenderer->GetBufferBuilder()
                    .SetSize(sizeof(glm::mat4) * mInstanceCapacity)
                    .SetUsage(fra::BufferUsage::Instance)
                    .Build();
        }

        mInstanceBuffer->Copy(mInstanceMatrices.data(), requiredSize);

        mRenderer->BindBuffer(mInstanceBuffer);

        for(const auto &draw: draws)
        {
            mRenderer->DrawInstanced(draw.meshId, draw.materialId, 1, draw.firstInstance,
                                     draw.entityId);
        }
    }

} // namespace FRIGGA_NAMESPACE
