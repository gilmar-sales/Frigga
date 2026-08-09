#include "RenderSystem.hpp"

#include "../Components/AnimatorComponent.hpp"
#include "../Components/CameraComponent.hpp"
#include "../Components/LightComponent.hpp"
#include "../Components/MaterialComponent.hpp"
#include "../Components/MeshComponent.hpp"
#include "../Components/TransformComponent.hpp"
#include "Frigga/Scene/Scene.hpp"

#include <Freya/Core/LightService.hpp>

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
        // LightService::Update (invoked by UpdateCamera) uploads the GPU UBO.
        // Sync ECS lights first so ClearLights/AddLight populate CPU state before upload.
        syncLights();
        updateCamera();
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
        if(mScene->IsUsingPreviewCamera())
        {
            const auto &previewCamera = mScene->GetPreviewCamera();
            applyCameraPose(previewCamera.transform, previewCamera.fovDegrees,
                            previewCamera.nearPlane, previewCamera.farPlane);
            return;
        }

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
                // Match Freya/OpenGL: entity local -Z is the aimed light direction / area normal.
                const glm::vec3 direction =
                    glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
                const glm::vec3 safeDirection =
                    glm::dot(direction, direction) > 1e-6f ? direction
                                                           : glm::vec3(0.0f, -1.0f, 0.0f);

                if(light.type == fra::LightType::Area)
                {
                    const glm::vec3 tangent =
                        transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                    fra::Light gpuLight = fra::MakeAreaLight(
                        transform.position, safeDirection, tangent, light.halfWidth,
                        light.halfHeight, light.color, light.intensity);
                    gpuLight.castShadows = light.castShadows;
                    mLightService->AddLight(gpuLight);
                    return;
                }

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
                gpuLight.castShadows = light.castShadows;

                mLightService->AddLight(gpuLight);
            });
    }

    void RenderSystem::drawMeshes()
    {
        mSceneInstances.clear();

        const bool isolate = mScene->IsUsingPreviewCamera() && mScene->HasRenderIsolation();
        const fr::Entity isolatedEntity = isolate ? mScene->GetRenderIsolation()
                                                  : static_cast<fr::Entity>(-1);

        mRegistry->CreateMutation()->Each<TransformComponent, MeshComponent, MaterialComponent>(
            [this, isolate, isolatedEntity](auto entity, TransformComponent &transform,
                                            MeshComponent &mesh, MaterialComponent &material) {
                if(isolate && entity != isolatedEntity)
                {
                    return;
                }

                glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
                model           = model * glm::mat4_cast(transform.rotation);
                model           = glm::scale(model, transform.scale);

                fra::SceneInstanceUpload upload {
                    .model       = model,
                    .meshId      = mesh.meshId,
                    .materialId  = material.materialId,
                    .entityId    = static_cast<std::uint32_t>(entity),
                    .castShadows = mesh.castShadows,
                };

                mRegistry->TryGetComponents<AnimatorComponent>(
                    entity, [&](AnimatorComponent &animator) {
                        if(animator.boneCount > 0 && animator.boneOffset != fra::kNoSkin)
                        {
                            upload.boneOffset = animator.boneOffset;
                            upload.boneCount  = animator.boneCount;
                        }
                    });

                mSceneInstances.push_back(upload);
            });

        // Prefer entityId order so Freya resolves TAA prevModel by entity.
        std::sort(mSceneInstances.begin(), mSceneInstances.end(),
                  [](const fra::SceneInstanceUpload &a, const fra::SceneInstanceUpload &b) {
                      if(a.meshId != b.meshId)
                      {
                          return a.meshId < b.meshId;
                      }
                      return a.entityId < b.entityId;
                  });

        mRenderer->UploadSceneInstances(mSceneInstances);
    }

} // namespace FRIGGA_NAMESPACE
