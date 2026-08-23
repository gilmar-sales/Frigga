#include "RenderSystem.hpp"

#include "../Components/AnimatorComponent.hpp"
#include "../Components/BillboardComponent.hpp"
#include "../Components/BillboardTextComponent.hpp"
#include "../Components/CameraComponent.hpp"
#include "../Components/FullscreenEffectComponent.hpp"
#include "../Components/HealthBarComponent.hpp"
#include "../Components/LightComponent.hpp"
#include "../Components/MaterialComponent.hpp"
#include "../Components/MeshComponent.hpp"
#include "../Components/ParticleEmitterComponent.hpp"
#include "../Components/TransformComponent.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Scene/Scene.hpp"

#include <Freya/Asset/FontAtlas.hpp>
#include <Freya/Core/LightService.hpp>

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
        /// Push constants for Shaders/Cell/cell.frag (std430). Matches the
        /// layout previously exposed by Freya's fullscreen-effect builder.
        struct CellPushConstants
        {
            float     bands           = 4.0f;
            float     edgeDepthScale  = 80.0f;
            float     edgeNormalScale = 2.0f;
            float     strength        = 1.0f;
            glm::vec4 edgeColor {0.02f, 0.02f, 0.04f, 1.0f};
            float     reverseZ   = 0.0f;
            float     shadowLift = 0.22f;
            float     edgeWidth  = 1.0f;
        };

        fra::Light MakeGpuLight(const TransformUtil::Pose &pose, const LightComponent &light)
        {
            // Match Freya/OpenGL: entity local -Z is the aimed light direction / area normal.
            const glm::vec3 direction =
                glm::normalize(pose.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            const glm::vec3 safeDirection =
                glm::dot(direction, direction) > 1e-6f ? direction : glm::vec3(0.0f, -1.0f, 0.0f);

            if(light.type == fra::LightType::Area)
            {
                const glm::vec3 tangent = pose.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                fra::Light gpuLight     = fra::MakeAreaLight(
                    pose.position, safeDirection, tangent, light.halfWidth, light.halfHeight,
                    light.color, light.intensity);
                gpuLight.castShadows = light.castShadows;
                return gpuLight;
            }

            fra::Light gpuLight {};
            gpuLight.position  = pose.position;
            gpuLight.type      = static_cast<float>(light.type);
            gpuLight.color     = light.color;
            gpuLight.radius    = light.radius;
            gpuLight.direction = safeDirection;
            gpuLight.intensity = light.intensity;
            gpuLight.innerCutoff =
                std::cos(glm::radians(std::max(light.innerAngleDegrees, 0.0f)));
            gpuLight.outerCutoff =
                std::cos(glm::radians(std::max(light.outerAngleDegrees, 0.0f)));
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
        (void)fovDegrees;
        (void)nearPlane;
        (void)farPlane;

        // Freya/OpenGL convention: camera looks along local -Z.
        const glm::vec3 forward =
            glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 up = glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        if(glm::dot(forward, forward) < 1e-6f || glm::dot(up, up) < 1e-6f)
        {
            return;
        }

        mRenderer->UpdateCamera(position, position + forward, up);

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
        mRegistry->CreateMutation()->Each<TransformComponent, CameraComponent>(
            [&applyCamera](auto entity, TransformComponent &, CameraComponent &camera) {
                if(camera.primary)
                {
                    applyCamera(entity, camera);
                }
            });

        // Fallback: locked Main Camera, then any camera.
        if(!updated)
        {
            mRegistry->CreateMutation()->Each<TransformComponent, CameraComponent>(
                [&applyCamera](auto entity, TransformComponent &, CameraComponent &camera) {
                    if(camera.locked)
                    {
                        applyCamera(entity, camera);
                    }
                });
        }

        if(!updated)
        {
            mRegistry->CreateMutation()->Each<TransformComponent, CameraComponent>(
                [&applyCamera](auto entity, TransformComponent &, CameraComponent &camera) {
                    applyCamera(entity, camera);
                });
        }
    }

    void RenderSystem::syncLights()
    {
        // Do not call ClearLights()/RemoveLight() every frame. Freya memcpy's an
        // empty UBO into every in-flight ring slot, so the GPU lighting pass
        // often samples zeros. Click/pick waitIdle then shows one correct frame.
        std::vector<fra::Light> wanted;
        mRegistry->CreateMutation()->Each<TransformComponent, LightComponent>(
            [&](auto entity, TransformComponent &, LightComponent &light) {
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

        fra::Light off {};
        current = mLightService->GetLightCount();
        for(std::uint32_t i = static_cast<std::uint32_t>(wanted.size()); i < current; ++i)
        {
            mLightService->UpdateLight(i, off);
        }
    }

    void RenderSystem::drawMeshes()
    {
        mSceneInstances.clear();

        const bool isolate = mScene->IsUsingPreviewCamera() && mScene->HasRenderIsolation();
        const fr::Entity isolatedEntity = isolate ? mScene->GetRenderIsolation()
                                                  : static_cast<fr::Entity>(-1);

        mRegistry->CreateMutation()->Each<TransformComponent, MeshComponent, MaterialComponent>(
            [this, isolate, isolatedEntity](auto entity, TransformComponent &,
                                            MeshComponent &mesh, MaterialComponent &material) {
                if(isolate && entity != isolatedEntity)
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

        const auto skip = [isolate, isolatedEntity](fr::Entity entity) {
            return isolate && entity != isolatedEntity;
        };

        mRegistry->CreateMutation()->Each<TransformComponent, BillboardComponent>(
            [&](auto entity, TransformComponent &, BillboardComponent &billboard) {
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

        mRegistry->CreateMutation()->Each<TransformComponent, HealthBarComponent>(
            [&](auto entity, TransformComponent &, HealthBarComponent &bar) {
                if(skip(entity))
                {
                    return;
                }
                const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                draw.HealthBar(pose.position + bar.offset, bar.width, bar.height,
                               std::clamp(bar.fill, 0.0f, 1.0f), bar.background, bar.foreground);
            });

        mRegistry->CreateMutation()->Each<TransformComponent, BillboardTextComponent>(
            [&](auto entity, TransformComponent &, BillboardTextComponent &label) {
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
        mRegistry->CreateMutation()->Each<TransformComponent, ParticleEmitterComponent>(
            [&](auto entity, TransformComponent &, ParticleEmitterComponent &source) {
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
        mRegistry->CreateMutation()->Each<FullscreenEffectComponent>(
            [&](auto entity, FullscreenEffectComponent &comp) {
                live.insert(entity);

                const auto stageName =
                    std::format("{}##{}", comp.name.empty() ? "Effect" : comp.name,
                                static_cast<std::uint32_t>(entity));

                auto &runtime = mEffects[entity];
                const auto previousName = runtime.stageName;
                const bool rebuild =
                    !runtime.effect || runtime.fragment != comp.fragment ||
                    previousName != stageName;
                if(rebuild)
                {
                    const bool cell = comp.fragment.find("cell.frag") != std::string::npos;
                    auto builder    = mEffectBuilder->SetName(stageName).SetFragment(comp.fragment);
                    if(cell)
                    {
                        builder
                            .SetInputs({fra::PostProcessInput::SceneColor, fra::PostProcessInput::Depth,
                                        fra::PostProcessInput::Normal})
                            .SetPushConstantSize(
                                static_cast<std::uint32_t>(sizeof(CellPushConstants)));
                    }
                    else
                    {
                        builder.SetInputs({fra::PostProcessInput::SceneColor}).SetPushConstantSize(0);
                    }

                    runtime.effect    = builder.Build();
                    runtime.fragment  = comp.fragment;
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
                if(comp.fragment.find("cell.frag") != std::string::npos)
                {
                    CellPushConstants cell {};
                    cell.bands           = comp.bands;
                    cell.edgeDepthScale  = comp.edgeDepthScale;
                    cell.edgeNormalScale = comp.edgeNormalScale;
                    cell.strength        = comp.strength;
                    cell.edgeColor       = comp.edgeColor;
                    cell.reverseZ        = (mFreyaOptions && mFreyaOptions->ReverseZ) ? 1.0f : 0.0f;
                    cell.shadowLift      = comp.shadowLift;
                    cell.edgeWidth       = comp.edgeWidth;
                    runtime.effect->SetPushConstants(cell);
                }
            });

        for(auto &[entity, runtime] : mEffects)
        {
            if(!live.contains(entity) && runtime.effect)
            {
                runtime.effect->SetEnabled(false);
            }
        }
    }

} // namespace FRIGGA_NAMESPACE
