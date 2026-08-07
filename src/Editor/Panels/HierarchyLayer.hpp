#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Frigga.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

class HierarchyLayer: public fg::Layer
{
  public:
    HierarchyLayer(skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                   skr::Arc<fg::PrimitiveMeshFactory> primitives,
                   skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                   skr::Arc<fg::SceneSimulationState> simulation, skr::Arc<fra::Window> window);
    ~HierarchyLayer() override = default;

    void createEmptyEntity();
    void createPrimitiveEntity(fg::PrimitiveType type);
    void createCameraEntity();
    void createLightEntity(fra::LightType type);
    void addRigidBodyToSelection();
    void addLightToSelection(fra::LightType type);
    void addLightToEntity(fr::Entity entity, fra::LightType type);
    void drawEntityNode(fr::Entity entity, fg::NameComponent &name);

    void drawComponents();

    void onUpdate() override;
    void onGui() override;

    static const char *getLightDisplayName(fra::LightType type);
    static const char *getLightIcon(fra::LightType type);

  private:
    enum class PendingTextureSlot
    {
        None,
        Albedo,
        Normal,
        Roughness,
        Emissive,
        Metalness,
    };

    [[nodiscard]] bool isEntityLocked(fr::Entity entity) const;
    void setPrimaryCamera(fr::Entity entity);
    [[nodiscard]] const char *resolveEntityIcon(fr::Entity entity) const;
    static fg::LightComponent makeDefaultLight(fra::LightType type);
    static fg::TransformComponent makeDefaultLightTransform(fra::LightType type);
    [[nodiscard]] fg::RigidBodyComponent makeDefaultRigidBody(fr::Entity entity) const;

    void drawTextureSlot(const char *label, PendingTextureSlot slot,
                         std::optional<std::uint32_t> &textureId, bool &changed);
    void requestTextureForSlot(PendingTextureSlot slot);
    void processPendingTextureImport();
    static void onTextureDialog(void *userdata, const char *const *filelist, int filter);

    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fra::Window> mWindow;
    fr::Entity nodeToRename;

    std::mutex mDialogMutex;
    PendingTextureSlot mPendingTextureSlot = PendingTextureSlot::None;
    std::optional<std::filesystem::path> mPendingTexturePath;
    fr::Entity mPendingTextureEntity = SelectionContext::Invalid;
};
