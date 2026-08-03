#pragma once

#include "Editor/SelectionContext.hpp"

#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <Frigga/Frigga.hpp>

#include <ImGuizmo.h>
#include <vulkan/vulkan.h>

class EditorLayer: public fg::Layer
{
  public:
    EditorLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                skr::Arc<SelectionContext> selection);
    ~EditorLayer() override = default;

    void onAttach() override;
    void onDettach() override;
    void onUpdate() override;
    void onGui() override;

  private:
    void ensureTarget(std::uint32_t width, std::uint32_t height);
    void releaseTexture();
    void recreateUiPipeline();
    void drawToolbar();
    void drawGizmos(const ImVec2 &imageMin, const ImVec2 &imageSize);
    [[nodiscard]] static glm::mat4 buildModelMatrix(const fg::TransformComponent &transform);
    static void applyModelMatrix(fg::TransformComponent &transform, const glm::mat4 &matrix);

    skr::Arc<fra::Renderer> mRenderer;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fra::RenderTarget> mTarget;
    VkDescriptorSet mTextureId = VK_NULL_HANDLE;
    std::uint32_t mWidth         = 0;
    std::uint32_t mHeight        = 0;
    std::uint32_t mPendingWidth  = 1280;
    std::uint32_t mPendingHeight = 720;
    bool mClaimOutput            = true;
    bool mViewportHovered        = false;

    ImGuizmo::OPERATION mOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE mMode           = ImGuizmo::LOCAL;
    bool mDrawGrid                 = true;
};
