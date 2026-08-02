#include "RenderSystem.hpp"

#include "../Components/CameraComponent.hpp"
#include "../Components/MeshComponent.hpp"
#include "../Components/TransformComponent.hpp"

#include <algorithm>

namespace FRIGGA_NAMESPACE
{

    RenderSystem::RenderSystem(const skr::Arc<fr::Registry> &registry,
                               const skr::Arc<fra::Renderer> &renderer,
                               const skr::Arc<fra::Window> &window)
        : System(registry), mRenderer(renderer), mWindow(window)
    {
    }

    void RenderSystem::Update(float deltaTime)
    {
        updateCamera();
        drawMeshes();
    }

    void RenderSystem::updateCamera()
    {
        bool updated = false;

        auto applyCamera = [this, &updated](TransformComponent &transform, CameraComponent &camera) {
            if(updated)
            {
                return;
            }

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

            auto projection           = mRenderer->GetCurrentProjection();
            projection.projection     = mRenderer->MakeProjection(
                glm::radians(camera.fovDegrees), aspect, camera.nearPlane, camera.farPlane);
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

    void RenderSystem::drawMeshes()
    {
        mInstanceMatrices.clear();

        struct DrawItem
        {
            std::uint32_t meshId;
            std::uint32_t materialId;
            std::uint32_t firstInstance;
        };
        std::vector<DrawItem> draws;

        mRegistry->CreateMutation()->Each<TransformComponent, MeshComponent>(
            [this, &draws](auto entity, TransformComponent &transform, MeshComponent &mesh) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
                model           = model * glm::mat4_cast(transform.rotation);
                model           = glm::scale(model, transform.scale);

                const auto firstInstance =
                    static_cast<std::uint32_t>(mInstanceMatrices.size());
                mInstanceMatrices.push_back(model);
                draws.push_back({mesh.meshId, mesh.materialId, firstInstance});
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
            mRenderer->DrawInstanced(draw.meshId, draw.materialId, 1, draw.firstInstance);
        }
    }

} // namespace FRIGGA_NAMESPACE
