#pragma once

#include "Editor/SelectionContext.hpp"
#include "Editor/Preferences/EditorPreferences.hpp"
#include "Editor/ViewportTarget.hpp"

#include <Frigga/Frigga.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/ECS/UserComponentRegistry.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

class GameplayLayer: public fg::Layer
{
  public:
    GameplayLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                  skr::Arc<fg::Scene> scene, skr::Arc<fg::PrimitiveMeshFactory> primitives,
                  skr::Arc<fg::SceneSimulationState> simulation,
                  skr::Arc<SelectionContext> selection,
                  skr::Arc<fg::IPhysicsWorld> physicsWorld,
                  skr::Arc<fg::UserComponentRegistry> userComponents,
                  skr::Arc<EditorPreferences> preferences, skr::Arc<fg::Input> input,
                  skr::Arc<fra::Window> window);
    ~GameplayLayer() override = default;

    void onAttach() override;
    void onDettach() override;
    void onSuspend() override;
    void onUpdate() override;
    void onGui() override;

  private:
    void drawToolbar();
    void drawColliders(const ImVec2 &imageMin, const ImVec2 &imageSize);
    void drawDebugOverlays(const ImVec2 &imageMin, const ImVec2 &imageSize);
    void syncMouseCapture();
    bool computeActiveCamera(glm::mat4 &viewOut, glm::mat4 &projectionOut);

    skr::Arc<fra::Renderer> mRenderer;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::IPhysicsWorld> mPhysicsWorld;
    skr::Arc<fg::UserComponentRegistry> mUserComponents;
    skr::Arc<EditorPreferences> mPreferences;
    skr::Arc<fg::Input> mInput;
    skr::Arc<fra::Window> mWindow;
    fg::ViewportTarget mViewport;
    std::uint32_t mPendingWidth    = 1280;
    std::uint32_t mPendingHeight   = 720;
    bool mClaimOutput              = false;
    bool mViewportHovered          = false;
    bool mMouseGrabbed             = false;
};
