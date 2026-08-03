#pragma once

#include <Frigga/Frigga.hpp>

#include <vulkan/vulkan.h>

class GameplayLayer: public fg::Layer
{
  public:
    explicit GameplayLayer(skr::Arc<fra::Renderer> renderer);
    ~GameplayLayer() override = default;

    void onAttach() override;
    void onDettach() override;
    void onUpdate() override;
    void onGui() override;

  private:
    void ensureTarget(std::uint32_t width, std::uint32_t height);
    void releaseTexture();
    void recreateUiPipeline();

    skr::Arc<fra::Renderer> mRenderer;
    skr::Arc<fra::RenderTarget> mTarget;
    VkDescriptorSet mTextureId = VK_NULL_HANDLE;
    std::uint32_t mWidth         = 0;
    std::uint32_t mHeight        = 0;
    std::uint32_t mPendingWidth  = 1280;
    std::uint32_t mPendingHeight = 720;
    bool mClaimOutput            = false;
};
