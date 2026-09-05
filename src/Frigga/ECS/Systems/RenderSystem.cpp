#include <Frigga/ECS/Systems/RenderSystem.hpp>

#include <Frigga/ECS/Systems/../Components/AnimatorComponent.hpp>
#include <Frigga/ECS/Systems/../Components/BillboardComponent.hpp>
#include <Frigga/ECS/Systems/../Components/BillboardTextComponent.hpp>
#include <Frigga/ECS/Systems/../Components/CameraComponent.hpp>
#include <Frigga/ECS/Systems/../Components/FullscreenEffectComponent.hpp>
#include <Frigga/ECS/Systems/../Components/HealthBarComponent.hpp>
#include <Frigga/ECS/Systems/../Components/HierarchyComponent.hpp>
#include <Frigga/ECS/Systems/../Components/LightComponent.hpp>
#include <Frigga/ECS/Systems/../Components/MaterialComponent.hpp>
#include <Frigga/ECS/Systems/../Components/MeshComponent.hpp>
#include <Frigga/ECS/Systems/../Components/ParticleEmitterComponent.hpp>
#include <Frigga/ECS/Systems/../Components/TransformComponent.hpp>
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Scene/Scene.hpp"

#include "Frigga/Rendering/FullscreenEffectCatalog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <unordered_set>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        bool IsInIsolatedSubtree(fr::Registry &registry, fr::Entity entity, fr::Entity root)
        {
            fr::Entity current = entity;
            while(current != static_cast<fr::Entity>(-1))
            {
                if(current == root)
                {
                    return true;
                }
                fr::Entity parent = static_cast<fr::Entity>(-1);
                registry.TryGetComponents<HierarchyComponent>(current, [&](HierarchyComponent &h) {
                    parent = h.parent;
                });
                current = parent;
            }
            return false;
        }
    } // namespace

    namespace
    {
        fra::Light MakeGpuLight(const TransformUtil::Pose &pose, const LightComponent &light)
        {
            // Match Freya/OpenGL: entity local -Z is the aimed light direction / area normal.
            const glm::vec3 direction =
                glm::normalize(pose.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            const glm::vec3 safeDirection =
                glm::dot(direction, direction) > 1e-6f ? direction : glm::vec3(0.0f, -1.0f, 0.0f);

            fra::Light gpuLight {};
            switch(light.type)
            {
            case fra::LightType::Point:
                gpuLight = fra::MakePointLight(pose.position, light.color, light.radius,
                                               light.intensity);
                break;
            case fra::LightType::Directional:
                gpuLight            = fra::MakeDirectionalLight(safeDirection, light.color,
                                                                light.intensity);
                gpuLight.position   = pose.position;
                break;
            case fra::LightType::Spot:
                gpuLight = fra::MakeSpotLight(
                    pose.position, safeDirection, light.color, light.radius,
                    glm::radians(std::max(light.innerAngleDegrees, 0.0f)),
                    glm::radians(std::max(light.outerAngleDegrees, 0.0f)), light.intensity);
                break;
            case fra::LightType::Area:
            {
                const glm::vec3 tangent = pose.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                gpuLight                = fra::MakeAreaLight(
                    pose.position, safeDirection, tangent, light.halfWidth, light.halfHeight,
                    light.color, light.intensity);
                break;
            }
            }

            gpuLight.castShadows = light.castShadows;
            return gpuLight;
        }
    } // namespace

    RenderSystem::RenderSystem(const skr::Arc<fr::Registry> &registry,
                               const skr::Arc<fra::Renderer> &renderer,
                               const skr::Arc<fra::Window> &window,
                               const skr::Arc<fra::LightService> &lightService,
                               const skr::Arc<Scene> &scene, const skr::Arc<AssetRegistry> &assets,
                               const skr::Arc<fra::FreyaOptions> &freyaOptions,
                               const skr::Arc<fra::TexturePool> &textures,
                               const skr::Arc<fra::PostProcessBuilder> &effectBuilder)
        : System(registry), mRenderer(renderer), mWindow(window), mLightService(lightService),
          mScene(scene), mAssets(assets), mFreyaOptions(freyaOptions), mTextures(textures),
          mEffectBuilder(effectBuilder)
    {
    }

    void RenderSystem::Update(float deltaTime)
    {
        // LightService::Update (invoked by UpdateCamera) uploads the GPU UBO
        // for the current frame only. Keep CPU lights in place first.
        syncLights();
        updateCamera();
        drawMeshes();
        drawBillboards(deltaTime);
        syncFullscreenEffects();
    }

    void RenderSystem::applyCameraPose(const glm::vec3 &position, const glm::quat &rotation,
                                       float fovDegrees, float nearPlane, float farPlane)
    {
        // Freya/OpenGL convention: camera looks along local -Z.
        const glm::vec3 forward =
            glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 up = glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        if(glm::dot(forward, forward) < 1e-6f || glm::dot(up, up) < 1e-6f)
        {
            return;
        }

        const float safeNear = std::max(nearPlane, 1.0e-3f);
        const float safeFar  = std::max(farPlane, safeNear + 1.0e-2f);
        const float safeFov  = std::clamp(fovDegrees, 1.0f, 179.0f);

        mRenderer->UpdateCamera(position, position + forward, up, glm::radians(safeFov),
                                safeNear, safeFar);

        // Feed ambient from options so the deferred pass keeps its intensity.
        if(mFreyaOptions)
        {
            mRenderer->SetAmbient(mFreyaOptions->ambientColor,
                                  mFreyaOptions->ambientIntensity);
        }
    }

    void RenderSystem::updateCamera()
    {
        if(mScene->IsUsingPreviewCamera())
        {
            const auto &previewCamera = mScene->GetPreviewCamera();
            applyCameraPose(previewCamera.transform.position, previewCamera.transform.rotation,
                            previewCamera.fovDegrees, previewCamera.nearPlane,
                            previewCamera.farPlane);
            return;
        }

        if(mScene->IsUsingEditorCamera())
        {
            const auto &editorCamera = mScene->GetEditorCamera();
            applyCameraPose(editorCamera.transform.position, editorCamera.transform.rotation,
                            editorCamera.fovDegrees, editorCamera.nearPlane,
                            editorCamera.farPlane);
            return;
        }

        bool updated = false;

        auto applyCamera = [this, &updated](fr::Entity entity, CameraComponent &camera) {
            if(updated)
            {
                return;
            }

            const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
            applyCameraPose(pose.position, pose.rotation, camera.fovDegrees, camera.nearPlane,
                            camera.farPlane);
            updated = true;
        };

        // Prefer an explicitly marked primary camera.
        mRegistry->CreateMutation()->Each(
            [&applyCamera](fr::Entity entity, TransformComponent &, CameraComponent &camera) {
                if(camera.primary)
                {
                    applyCamera(entity, camera);
                }
            });

        // Fallback: locked Main Camera, then any camera.
        if(!updated)
        {
            mRegistry->CreateMutation()->Each(
                [&applyCamera](fr::Entity entity, TransformComponent &, CameraComponent &camera) {
                    if(camera.locked)
                    {
                        applyCamera(entity, camera);
                    }
                });
        }

        if(!updated)
        {
            mRegistry->CreateMutation()->Each(
                [&applyCamera](fr::Entity entity, TransformComponent &, CameraComponent &camera) {
                    applyCamera(entity, camera);
                });
        }
    }

    void RenderSystem::syncLights()
    {
        // Do not call ClearLights()/RemoveLight() every frame. Freya memcpy's an
        // empty UBO into every in-flight ring slot, so the GPU lighting pass
        // often samples zeros. Click/pick waitIdle then shows one correct frame.
        const bool isolate = mScene->IsUsingPreviewCamera() && mScene->HasRenderIsolation();
        const fr::Entity isolatedEntity = isolate ? mScene->GetRenderIsolation()
                                                    : static_cast<fr::Entity>(-1);

        std::vector<fra::Light> wanted;
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, LightComponent &light) {
                if(isolate && !IsInIsolatedSubtree(*mRegistry, entity, isolatedEntity))
                {
                    return;
                }
                wanted.push_back(MakeGpuLight(TransformUtil::WorldPose(*mRegistry, entity), light));
            });

        const auto maxLights = mLightService->GetMaxLights();
        if(wanted.size() > maxLights)
        {
            wanted.resize(maxLights);
        }

        auto current = mLightService->GetLightCount();
        for(std::uint32_t i = 0; i < wanted.size(); ++i)
        {
            if(i < current)
            {
                mLightService->UpdateLight(i, wanted[i]);
            }
            else
            {
                mLightService->AddLight(wanted[i]);
            }
        }

        while(mLightService->GetLightCount() > wanted.size())
        {
            mLightService->RemoveLight(mLightService->GetLightCount() - 1U);
        }
    }

    void RenderSystem::drawMeshes()
    {
        mSceneInstances.clear();

        const bool isolate = mScene->IsUsingPreviewCamera() && mScene->HasRenderIsolation();
        const fr::Entity isolatedEntity = isolate ? mScene->GetRenderIsolation()
                                                  : static_cast<fr::Entity>(-1);

        mRegistry->CreateMutation()->Each(
            [this, isolate, isolatedEntity](fr::Entity entity, TransformComponent &,
                                            MeshComponent &mesh, MaterialComponent &material) {
                if(isolate && !IsInIsolatedSubtree(*mRegistry, entity, isolatedEntity))
                {
                    return;
                }

                const glm::mat4 model = TransformUtil::WorldMatrix(*mRegistry, entity);

                fra::SceneInstanceUpload upload {
                    .model       = model,
                    .meshId      = mesh.meshId,
                    .materialId  = material.materialId,
                    .entityId    = static_cast<std::uint32_t>(entity),
                    .castShadows = mesh.castShadows,
                };

                mRegistry->TryGetComponents<AnimatorComponent>(
                    entity, [&](AnimatorComponent &animator) {
                        if(animator.boneCount > 0 && animator.boneOffset != fra::kNoSkin)
                        {
                            upload.boneOffset = animator.boneOffset;
                            upload.boneCount  = animator.boneCount;
                        }
                    });

                mSceneInstances.push_back(upload);
            });

        // Prefer entityId order so Freya resolves TAA prevModel by entity.
        std::sort(mSceneInstances.begin(), mSceneInstances.end(),
                  [](const fra::SceneInstanceUpload &a, const fra::SceneInstanceUpload &b) {
                      if(a.meshId != b.meshId)
                      {
                          return a.meshId < b.meshId;
                      }
                      return a.entityId < b.entityId;
                  });

        mRenderer->UploadSceneInstances(mSceneInstances);
    }

    std::uint32_t RenderSystem::textureHeapIndex(std::optional<std::uint32_t> textureId) const
    {
        if(!textureId || *textureId == 0)
        {
            return 0;
        }
        // Bindless heap slots 0/1 are reserved (white/black); TexturePool ids
        // start at 2, so the heap index is simply the texture id.
        return *textureId + 2;
    }

    const fra::FontAtlas *RenderSystem::fontFor(const std::string &relativePath)
    {
        if(!mTextures || relativePath.empty())
        {
            return nullptr;
        }

        auto [it, inserted] = mFonts.try_emplace(relativePath);
        if(inserted)
        {
            const auto absolute = AssetRegistry::ToAbsoluteResourcePath(relativePath);
            it->second          = fra::FontAtlas::Create(*mTextures, absolute.string());
        }
        return it->second.Valid() ? &it->second : nullptr;
    }

    void RenderSystem::drawBillboards(float deltaTime)
    {
        if(!mRenderer)
        {
            return;
        }

        auto &draw = mRenderer->GetBillboardDraw();
        const bool isolate = mScene->IsUsingPreviewCamera() && mScene->HasRenderIsolation();
        const fr::Entity isolatedEntity = isolate ? mScene->GetRenderIsolation()
                                                  : static_cast<fr::Entity>(-1);

        const auto skip = [this, isolate, isolatedEntity](fr::Entity entity) {
            return isolate && !IsInIsolatedSubtree(*mRegistry, entity, isolatedEntity);
        };

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, BillboardComponent &billboard) {
                if(skip(entity))
                {
                    return;
                }
                const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                fra::Billboard quad {};
                quad.worldPos      = pose.position;
                quad.size          = {billboard.size.x * std::abs(pose.scale.x),
                                      billboard.size.y * std::abs(pose.scale.y)};
                quad.color         = billboard.color;
                quad.uvRect        = billboard.uvRect;
                quad.textureIndex  = textureHeapIndex(billboard.textureId);
                quad.align         = billboard.align;
                quad.blend         = billboard.blend;
                quad.layer         = billboard.layer;
                quad.depthTest     = billboard.depthTest;
                quad.sdf           = billboard.sdf;
                quad.clipMax       = billboard.clipMax;
                quad.localOffset   = billboard.localOffset;
                draw.Quad(quad);
            });

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, HealthBarComponent &bar) {
                if(skip(entity))
                {
                    return;
                }
                const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                draw.HealthBar(pose.position + bar.offset, bar.width, bar.height,
                               std::clamp(bar.fill, 0.0f, 1.0f), bar.background, bar.foreground);
            });

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, BillboardTextComponent &label) {
                if(skip(entity) || label.text.empty())
                {
                    return;
                }
                const auto *font = fontFor(label.fontSource);
                if(font == nullptr)
                {
                    return;
                }
                const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                draw.Text(pose.position + label.offset, label.text, *font, label.heightMeters,
                          label.color, label.borderWidth, label.borderColor, label.align,
                          label.layer);
            });

        std::unordered_set<fr::Entity> liveEmitters;
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, ParticleEmitterComponent &source) {
                if(skip(entity))
                {
                    return;
                }
                liveEmitters.insert(entity);
                auto &emitter          = mEmitters[entity];
                emitter.origin         = TransformUtil::WorldPose(*mRegistry, entity).position;
                emitter.velocity       = source.velocity;
                emitter.velocityJitter = source.velocityJitter;
                emitter.spawnRate      = source.playing ? source.spawnRate : 0.0f;
                emitter.lifetime       = source.lifetime;
                emitter.size0          = source.size0;
                emitter.size1          = source.size1;
                emitter.color0         = source.color0;
                emitter.color1         = source.color1;
                emitter.blend          = source.blend;
                emitter.textureIndex   = textureHeapIndex(source.textureId);
                emitter.maxParticles   = source.maxParticles;
                emitter.Tick(deltaTime, draw);
            });
        std::erase_if(mEmitters, [&](const auto &entry) {
            return !liveEmitters.contains(entry.first);
        });
    }

    void RenderSystem::syncFullscreenEffects()
    {
        if(!mRenderer || !mEffectBuilder)
        {
            return;
        }

        std::unordered_set<fr::Entity> live;
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, FullscreenEffectComponent &comp) {
                live.insert(entity);

                const auto stageName =
                    std::format("{}##{}", comp.name.empty() ? "Effect" : comp.name,
                                static_cast<std::uint32_t>(entity));

                auto &runtime = mEffects[entity];
                const auto previousName = runtime.stageName;
                const bool rebuild =
                    !runtime.effect || runtime.fragment != comp.fragment ||
                    runtime.kind != comp.kind || previousName != stageName;
                if(rebuild)
                {
                    ConfigureFullscreenEffectBuilder(*mEffectBuilder, stageName, comp);
                    runtime.effect    = mEffectBuilder->Build();
                    runtime.fragment  = comp.fragment;
                    runtime.kind      = comp.kind;
                    runtime.stageName = stageName;
                    if(runtime.effect)
                    {
                        auto stage = runtime.effect->MakeStage();
                        const bool replaced =
                            !previousName.empty() &&
                            mRenderer->ReplaceFrameStage(previousName.c_str(), stage);
                        if(!replaced)
                        {
                            mRenderer->InsertFrameStage("BillboardVfx", std::move(stage));
                        }
                    }
                }

                if(!runtime.effect)
                {
                    return;
                }

                runtime.effect->SetEnabled(comp.enabled);
                if(comp.enabled)
                {
                    mEffectTimeSec[entity] += mWindow->GetDeltaTime();
                }

                const FullscreenEffectPushState pushState {
                    .timeSec  = mEffectTimeSec[entity],
                    .reverseZ = mFreyaOptions && mFreyaOptions->ReverseZ,
                    .component = &comp,
                };
                ApplyFullscreenEffectPushConstants(*runtime.effect, pushState);
                SyncFullscreenEffectMaterials(*runtime.effect, comp);
            });

        for(auto &[entity, runtime] : mEffects)
        {
            if(!live.contains(entity) && runtime.effect)
            {
                runtime.effect->SetEnabled(false);
            }
        }

        std::erase_if(mEffectTimeSec, [&](const auto &entry) { return !live.contains(entry.first); });
    }

} // namespace FRIGGA_NAMESPACE
