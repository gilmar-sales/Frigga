#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Animation/AnimationController.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Core/Layer.hpp>
#include <Freyr/Freyr.hpp>

#include <imgui.h>

class AnimGraphEditorLayer: public fg::Layer
{
  public:
    AnimGraphEditorLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                         skr::Arc<fr::Registry> registry,
                         skr::Arc<fg::SceneSimulationState> simulation,
                         skr::Arc<fg::AnimationController> controller);
    ~AnimGraphEditorLayer() override = default;

    void onGui() override;

  private:
    void drawCanvas(fg::AnimatorComponent &animator, const fg::ModelAsset *model);
    void drawInspector(fg::AnimatorComponent &animator, const fg::ModelAsset *model);
    void drawStateNode(ImDrawList *drawList, const ImVec2 &origin, std::size_t index,
                       fg::AnimGraphStateDef &state, std::string_view currentState,
                       std::string_view nextState, bool playing);
    void handleCanvasInput(fg::AnimatorComponent &animator, const ImVec2 &origin,
                           const ImVec2 &canvasMin, const ImVec2 &canvasMax);

    [[nodiscard]] ImVec2 worldToScreen(const ImVec2 &origin, float x, float y) const;
    [[nodiscard]] ImVec2 screenToWorld(const ImVec2 &origin, const ImVec2 &screen) const;
    [[nodiscard]] ImVec2 nodeSize() const;
    [[nodiscard]] ImVec2 inPinPos(const ImVec2 &origin, const fg::AnimGraphStateDef &state) const;
    [[nodiscard]] ImVec2 outPinPos(const ImVec2 &origin, const fg::AnimGraphStateDef &state) const;

    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fg::AnimationController> mController;

    ImVec2 mPan {0.0f, 0.0f};
    float  mZoom = 1.0f;
    int    mSelectedState      = -1;
    int    mSelectedTransition = -1;
    int    mLinkFrom           = -1;
    int    mDraggingState      = -1;
    bool   mPanning            = false;
    ImVec2 mDragOffset {0.0f, 0.0f};
    fr::Entity mTrackedEntity = SelectionContext::Invalid;
};
