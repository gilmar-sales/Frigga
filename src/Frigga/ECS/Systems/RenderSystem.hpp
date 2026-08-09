#pragma once

#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <Freya/Vulkan.hpp>
#include <Freyr/Freyr.hpp>

#include <vector>

namespace FRIGGA_NAMESPACE
{

    class Scene;

    class RenderSystem: public fr::System
    {
      public:
        RenderSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fra::Renderer> &renderer,
                     const skr::Arc<fra::Window> &window,
                     const skr::Arc<fra::LightService> &lightService,
                     const skr::Arc<Scene> &scene);

        ~RenderSystem() override = default;

        void Update(float deltaTime) override;

      private:
        void updateCamera();
        void applyCameraPose(const TransformComponent &transform, float fovDegrees,
                             float nearPlane, float farPlane);
        void syncLights();
        void drawMeshes();

        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<fra::Window> mWindow;
        skr::Arc<fra::LightService> mLightService;
        skr::Arc<Scene> mScene;
        std::vector<fra::SceneInstanceUpload> mSceneInstances;
    };

} // namespace FRIGGA_NAMESPACE
