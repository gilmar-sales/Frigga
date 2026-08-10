#pragma once

#include "Editor/SelectionContext.hpp"
#include "Editor/Preferences/EditorPreferences.hpp"

#include <Frigga/Frigga.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <vulkan/vulkan.h>

class GameplayLayer: public fg::Layer
{
  public:
    GameplayLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                  skr::Arc<fg::Scene> scene, skr::Arc<fg::PrimitiveMeshFactory> primitives,
                  skr::Arc<fg::SceneSimulationState> simulation,
                  skr::Arc<SelectionContext> selection, skr::Arc<fg::IPhysicsWorld> physicsWorld,
                  skr::Arc<EditorPreferences> preferences);
    ~GameplayLayer() override = default;

    void onAttach() override;
    void onDettach() override;
    void onUpdate() override;
    void onGui() override;

  private:
    void drawToolbar();
    void drawColliders(const ImVec2 &imageMin, const ImVec2 &imageSize);
    void ensureTarget(std::uint32_t width, std::uint32_t height);
    void releaseTexture();
    void recreateUiPipeline();

    skr::Arc<fra::Renderer> mRenderer;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::IPhysicsWorld> mPhysicsWorld;
    skr::Arc<EditorPreferences> mPreferences;
    skr::Arc<fra::RenderTarget> mTarget;
    VkDescriptorSet mTextureId = VK_NULL_HANDLE;
    std::uint32_t mWidth         = 0;
    std::uint32_t mHeight        = 0;
    std::uint32_t mPendingWidth  = 1280;
    std::uint32_t mPendingHeight = 720;
    bool mClaimOutput            = false;
};
