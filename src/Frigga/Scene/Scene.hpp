#pragma once

#include <Freya/Core/Renderer.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

namespace FRIGGA_NAMESPACE
{

    class Scene
    {
      public:
        Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
              const skr::Arc<fr::Registry> &ecsRegistry);
        ~Scene() = default;

        void Update(float ts);

        void OnEditorRender(float ts);

      private:
        skr::Arc<fr::Registry> mEcsRegistry;
        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<skr::Logger<Scene>> mLogger;
    };

} // namespace FRIGGA_NAMESPACE
