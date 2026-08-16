#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Frigga.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

class HierarchyLayer: public fg::Layer
{
  public:
    HierarchyLayer(skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                   skr::Arc<fg::PrimitiveMeshFactory> primitives,
                   skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                   skr::Arc<fg::SceneSimulationState> simulation, skr::Arc<fra::Window> window,
                   skr::Arc<fg::UserComponentRegistry> userComponents);
    ~HierarchyLayer() override = default;

    void createEmptyEntity();
    void createPrimitiveEntity(fg::PrimitiveType type);
    void createCameraEntity();
    void createLightEntity(fra::LightType type);
    void createBillboardEntity();
    void createParticleEntity();
    void createFullscreenEffectEntity();
    void addRigidBodyToSelection();
    void addCharacterControllerToSelection();
    void addThirdPersonCameraToSelection();
    void addBillboardToSelection();
    void addParticleEmitterToSelection();
    void addHealthBarToSelection();
    void addBillboardTextToSelection();
    void addFullscreenEffectToSelection();
    void addLightToSelection(fra::LightType type);
    void addLightToEntity(fr::Entity entity, fra::LightType type);
    void addUserComponentToSelection(std::string_view typeId);
    void addUserComponentToEntity(fr::Entity entity, std::string_view typeId);
    [[nodiscard]] bool hasUserComponentType(std::string_view typeId) const;
    void parentNewEntity(fr::Entity entity);
    void drawEntityNode(fr::Entity entity, fg::NameComponent &name);

    void drawComponents();
    void drawGameplayAddComponentMenu(fr::Entity entity);

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
        Billboard,
        Particle,
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
    skr::Arc<fg::UserComponentRegistry> mUserComponents;
    fr::Entity nodeToRename;

    std::mutex mDialogMutex;
    PendingTextureSlot mPendingTextureSlot = PendingTextureSlot::None;
    std::optional<std::filesystem::path> mPendingTexturePath;
    fr::Entity mPendingTextureEntity = SelectionContext::Invalid;
};
