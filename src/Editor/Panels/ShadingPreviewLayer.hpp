#pragma once

#include "Editor/MaterialSelectionContext.hpp"
#include "Editor/Preferences/EditorPreferences.hpp"
#include "Editor/ViewportTarget.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Frigga.hpp>

#include <imgui.h>

class ShadingPreviewLayer: public fg::Layer
{
  public:
    ShadingPreviewLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                        skr::Arc<fra::MeshPool> meshPool,
                        skr::Arc<fg::PrimitiveMeshFactory> primitives,
                        skr::Arc<MaterialSelectionContext> materialSelection,
                        skr::Arc<fg::Scene> scene, skr::Arc<EditorPreferences> preferences,
                        skr::Arc<fg::SceneSimulationState> simulation);
    ~ShadingPreviewLayer() override = default;

    void onAttach() override;
    void onDettach() override;
    void onSuspend() override;
    void onUpdate() override;
    void onGuiBegin() override;
    void onGuiEnd() override;

  private:
    void ensurePreviewScene();
    void syncPreviewMaterial();
    void syncCamera();
    void handleOrbit();

    skr::Arc<fra::Renderer> mRenderer;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fra::MeshPool> mMeshPool;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<MaterialSelectionContext> mMaterialSelection;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<EditorPreferences> mPreferences;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    fg::ViewportTarget mViewport;

    fr::Entity mPreviewRoot   = static_cast<fr::Entity>(-1);
    fr::Entity mPreviewSphere = static_cast<fr::Entity>(-1);
    bool mPreviewBuilt          = false;

    std::uint32_t mPendingWidth  = 1280;
    std::uint32_t mPendingHeight = 720;
    bool mClaimOutput            = true;
    bool mPreviewWindowOpen      = false;
    bool mPreviewWindowBegun     = false;
    ImVec2 mLayoutAvail {};
    bool mViewportHovered        = false;

    glm::vec3 mPivot {0.0f, 0.75f, 0.0f};
    float mDistance         = 2.8f;
    float mMinDistance      = 0.5f;
    float mMaxDistance      = 12.0f;
    float mYawDegrees       = 35.0f;
    float mPitchDegrees     = 20.0f;
    float mOrbitSensitivity = 0.35f;
    float mFovDegrees       = 40.0f;
};
