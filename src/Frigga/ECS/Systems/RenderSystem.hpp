#pragma once

#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class RenderSystem: public fr::System
    {
      public:
        RenderSystem(const Ref<fr::Scene> &scene, const Ref<fra::Renderer> &renderer, const Ref<fra::Window> &window)
            : System(scene), mRenderer(renderer), mWindow(window) {};

        ~RenderSystem() = default;

        void Update(float deltaTime) override;

      private:
        Ref<fra::Renderer> mRenderer;
        Ref<fra::Window> mWindow;
    };

} // namespace FRIGGA_NAMESPACE
