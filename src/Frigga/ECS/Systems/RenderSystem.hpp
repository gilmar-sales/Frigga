#pragma once

#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class RenderSystem: public fr::System
    {
      public:
        RenderSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fra::Renderer> &renderer,
                     const skr::Arc<fra::Window> &window)
            : System(registry), mRenderer(renderer), mWindow(window) {};

        ~RenderSystem() = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<fra::Window> mWindow;
    };

} // namespace FRIGGA_NAMESPACE
