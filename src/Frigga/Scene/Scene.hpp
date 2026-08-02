#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"

#include <Freya/Core/LightService.hpp>
#include <Freya/Core/Renderer.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

namespace FRIGGA_NAMESPACE
{

    class Scene
    {
      public:
        Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
              const skr::Arc<fr::Registry> &ecsRegistry,
              const skr::Arc<fra::LightService> &lightService,
              const skr::Arc<PrimitiveMeshFactory> &primitives);
        ~Scene() = default;

        void Update(float ts);
        void OnEditorRender(float ts);

        [[nodiscard]] fr::Entity GetMainCameraEntity() const
        {
            return mMainCameraEntity;
        }

        [[nodiscard]] bool IsMainCamera(fr::Entity entity) const
        {
            return entity == mMainCameraEntity;
        }

      private:
        skr::Arc<fr::Registry> mEcsRegistry;
        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<skr::Logger<Scene>> mLogger;
        skr::Arc<fra::LightService> mLightService;
        skr::Arc<PrimitiveMeshFactory> mPrimitives;
        fr::Entity mMainCameraEntity {};
    };

} // namespace FRIGGA_NAMESPACE
