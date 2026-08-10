#pragma once

#include "Editor/SelectionContext.hpp"
#include "Editor/Preferences/EditorPreferences.hpp"
#include "Frigga/Scene/Scene.hpp"

#include <Frigga/Frigga.hpp>

#include <cstdint>
#include <vulkan/vulkan.h>

class AnimationPreviewLayer: public fg::Layer
{
  public:
    AnimationPreviewLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                          skr::Arc<fra::MeshPool> meshPool, skr::Arc<SelectionContext> selection,
                          skr::Arc<fg::Scene> scene, skr::Arc<EditorPreferences> preferences);
    ~AnimationPreviewLayer() override = default;

    void onAttach() override;
    void onDettach() override;
    void onUpdate() override;
    void onGui() override;

  private:
    struct FrameBounds
    {
        glm::vec3 center {0.0f};
        float     radius = 1.0f;
    };

    void ensureTarget(std::uint32_t width, std::uint32_t height);
    void releaseTexture();
    void recreateUiPipeline();
    void syncCameraToSelection();
    void handleOrbit();
    [[nodiscard]] FrameBounds computeSelectionBounds(fr::Entity entity,
                                                     const fg::TransformComponent &transform) const;
    void applyFrame(const FrameBounds &bounds, bool resetOrbit);

    skr::Arc<fra::Renderer> mRenderer;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fra::MeshPool> mMeshPool;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<EditorPreferences> mPreferences;
    skr::Arc<fra::RenderTarget> mTarget;

    VkDescriptorSet mTextureId   = VK_NULL_HANDLE;
    std::uint32_t mWidth         = 0;
    std::uint32_t mHeight        = 0;
    std::uint32_t mPendingWidth  = 1280;
    std::uint32_t mPendingHeight = 720;
    bool mClaimOutput            = true;
    bool mViewportHovered        = false;

    fr::Entity mFramedEntity = SelectionContext::Invalid;
    glm::vec3 mPivot {0.0f, 0.0f, 0.0f};
    float mDistance         = 3.0f;
    float mMinDistance      = 0.2f;
    float mMaxDistance      = 50.0f;
    float mYawDegrees       = 35.0f;
    float mPitchDegrees     = 20.0f;
    float mOrbitSensitivity = 0.35f;
    float mFovDegrees       = 40.0f;
};
