#pragma once

#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>

#include <vector>

namespace FRIGGA_NAMESPACE
{

    class RenderSystem: public fr::System
    {
      public:
        RenderSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fra::Renderer> &renderer,
                     const skr::Arc<fra::Window> &window);

        ~RenderSystem() override = default;

        void Update(float deltaTime) override;

      private:
        void updateCamera();
        void drawMeshes();

        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<fra::Window> mWindow;
        skr::Arc<fra::Buffer> mInstanceBuffer;
        std::vector<glm::mat4> mInstanceMatrices;
        std::uint64_t mInstanceCapacity = 0;
    };

} // namespace FRIGGA_NAMESPACE
