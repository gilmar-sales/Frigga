#pragma once

#include "Editor/SelectionContext.hpp"

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Frigga.hpp>

#include <ImGuizmo.h>
#include <vulkan/vulkan.h>

class EditorLayer: public fg::Layer
{
  public:
    EditorLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                skr::Arc<fg::PrimitiveMeshFactory> primitives, skr::Arc<SelectionContext> selection,
                skr::Arc<fg::Scene> scene, skr::Arc<fg::SceneSimulationState> simulation);
    ~EditorLayer() override = default;

    void onAttach() override;
    void onDettach() override;
    void onUpdate() override;
    void onGui() override;

  private:
    enum class NavMode
    {
        None,
        Fly,
        Orbit,
        Pan
    };

    void ensureTarget(std::uint32_t width, std::uint32_t height);
    void releaseTexture();
    void recreateUiPipeline();
    void drawToolbar();
    void drawGizmos(const ImVec2 &imageMin, const ImVec2 &imageSize, bool allowManipulate);
    void handleNavigation();
    void handlePicking(const ImVec2 &imageMin, const ImVec2 &imageSize);
    void consumePickResult();
    [[nodiscard]] static glm::mat4 gizmoProjection(const glm::mat4 &vulkanProjection);
    void syncOrbitPivot(const fg::TransformComponent &camera);
    void applyYawPitch(fg::TransformComponent &camera, float yawDegrees, float pitchDegrees) const;
    void extractYawPitch(const fg::TransformComponent &camera, float &yawDegrees,
                         float &pitchDegrees) const;
    [[nodiscard]] static glm::vec3 cameraForward(const fg::TransformComponent &camera);
    [[nodiscard]] static glm::vec3 cameraRight(const fg::TransformComponent &camera);
    [[nodiscard]] static glm::vec3 cameraUp(const fg::TransformComponent &camera);
    [[nodiscard]] static glm::mat4 buildModelMatrix(const fg::TransformComponent &transform);
    static void applyModelMatrix(fg::TransformComponent &transform, const glm::mat4 &matrix);

    skr::Arc<fra::Renderer> mRenderer;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fra::RenderTarget> mTarget;
    VkDescriptorSet mTextureId = VK_NULL_HANDLE;
    std::uint32_t mWidth         = 0;
    std::uint32_t mHeight        = 0;
    std::uint32_t mPendingWidth  = 1280;
    std::uint32_t mPendingHeight = 720;
    bool mClaimOutput            = true;
    bool mViewportHovered        = false;
    bool mViewportFocused        = false;

    ImGuizmo::OPERATION mOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE mMode           = ImGuizmo::LOCAL;
    bool mDrawGrid                 = true;

    NavMode mNavMode           = NavMode::None;
    glm::vec3 mOrbitPivot {0.0f, 0.0f, 0.0f};
    float mOrbitDistance       = 8.0f;
    float mFlyYawDegrees       = 0.0f;
    float mFlyPitchDegrees     = 0.0f;
    float mMoveSpeed           = 5.0f;
    float mLookSensitivity     = 0.18f;
    float mOrbitSensitivity    = 0.25f;
};
