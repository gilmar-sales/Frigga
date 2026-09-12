#pragma once

#include "Frigga/ECS/Components/FullscreenEffectComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <Freya/Advanced.hpp>
#include <Freya/Asset/FontAtlas.hpp>
#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    class Scene;
    class AssetRegistry;

    class RenderSystem: public fr::System
    {
      public:
        RenderSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fra::Renderer> &renderer,
                     const skr::Arc<fra::Window> &window,
                     const skr::Arc<fra::LightService> &lightService,
                     const skr::Arc<Scene> &scene, const skr::Arc<AssetRegistry> &assets,
                     const skr::Arc<fra::FreyaOptions> &freyaOptions,
                     const skr::Arc<fra::TexturePool> &textures,
                     const skr::Arc<fra::PostProcessBuilder> &effectBuilder);

        ~RenderSystem() override = default;

        void Update(float deltaTime) override;

      private:
        void updateCamera();
        void applyCameraPose(const glm::vec3 &position, const glm::quat &rotation, float fovDegrees,
                             float nearPlane, float farPlane);
        void syncLights();
        void drawMeshes();
        void drawBillboards(float deltaTime);
        void syncFullscreenEffects();

        [[nodiscard]] std::uint32_t textureHeapIndex(std::optional<std::uint32_t> textureId) const;
        const fra::FontAtlas *fontFor(const std::string &relativePath);

        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<fra::Window> mWindow;
        skr::Arc<fra::LightService> mLightService;
        skr::Arc<Scene> mScene;
        skr::Arc<AssetRegistry> mAssets;
        skr::Arc<fra::FreyaOptions> mFreyaOptions;
        skr::Arc<fra::TexturePool> mTextures;
        skr::Arc<fra::PostProcessBuilder> mEffectBuilder;
        std::vector<fra::SceneInstanceUpload> mSceneInstances;
        /// Font atlases keyed by asset path (not per-entity).
        std::unordered_map<std::string, fra::FontAtlas> mFonts;
    };

} // namespace FRIGGA_NAMESPACE
