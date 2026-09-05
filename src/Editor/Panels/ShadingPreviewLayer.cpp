#include "ShadingPreviewLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/EditorViewportHost.hpp"
#include "Editor/ViewportDpi.hpp"
#include "Editor/ViewportQuality.hpp"
#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Gui/Backends/imgui_impl_vulkan.h"
#include "Frigga/Gui/GuiLayer.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPitchLimitDegrees = 85.0f;
} // namespace

ShadingPreviewLayer::ShadingPreviewLayer(
    skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
    skr::Arc<fra::MeshPool> meshPool, skr::Arc<fg::PrimitiveMeshFactory> primitives,
    skr::Arc<MaterialSelectionContext> materialSelection, skr::Arc<fg::Scene> scene,
    skr::Arc<EditorPreferences> preferences, skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Preview"), mRenderer(std::move(renderer)), mRegistry(std::move(registry)),
      mMeshPool(std::move(meshPool)), mPrimitives(std::move(primitives)),
      mMaterialSelection(std::move(materialSelection)), mScene(std::move(scene)),
      mPreferences(std::move(preferences)), mSimulation(std::move(simulation)),
      mViewport(mRenderer)
{
}

void ShadingPreviewLayer::onAttach()
{
    ensurePreviewScene();
}

void ShadingPreviewLayer::onDettach()
{
    if(mPreviewBuilt && mPreviewRoot != static_cast<fr::Entity>(-1))
    {
        fg::TransformUtil::DestroySubtree(*mRegistry, mPreviewRoot);
        mPreviewBuilt   = false;
        mPreviewRoot    = static_cast<fr::Entity>(-1);
        mPreviewSphere  = static_cast<fr::Entity>(-1);
    }
    mViewport.Release();
    mScene->ClearRenderIsolation();
}

void ShadingPreviewLayer::onSuspend()
{
    mClaimOutput     = false;
    mViewportHovered = false;
    mViewport.Suspend();
    mScene->ClearRenderIsolation();
}

void ShadingPreviewLayer::ensurePreviewScene()
{
    if(mPreviewBuilt)
    {
        return;
    }

    mPreviewRoot = mRegistry->CreateEntity(
        fg::NameComponent {.name = "__ShadingPreviewRoot"}, fg::TransformComponent {});

    mPreviewSphere = mRegistry->CreateEntity(
        fg::NameComponent {.name = "__ShadingPreviewSphere"},
        fg::TransformComponent {.position = {0.0f, 0.75f, 0.0f}, .scale = {1.0f, 1.0f, 1.0f}},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Sphere)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    fg::TransformUtil::SetParent(*mRegistry, mPreviewSphere, mPreviewRoot, true);

    const auto ground = mRegistry->CreateEntity(
        fg::NameComponent {.name = "__ShadingPreviewGround"},
        fg::TransformComponent {.scale = {4.0f, 1.0f, 4.0f}},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Plane)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    fg::TransformUtil::SetParent(*mRegistry, ground, mPreviewRoot, true);

    const auto keyLight = mRegistry->CreateEntity(
        fg::NameComponent {.name = "__ShadingPreviewKeyLight"},
        fg::TransformComponent {.position = {2.5f, 3.5f, 2.0f}},
        fg::LightComponent {.type = fra::LightType::Point, .radius = 20.0f, .intensity = 45.0f});
    fg::TransformUtil::SetParent(*mRegistry, keyLight, mPreviewRoot, true);

    const auto fillLight = mRegistry->CreateEntity(
        fg::NameComponent {.name = "__ShadingPreviewFillLight"},
        fg::TransformComponent {.position = {-2.0f, 2.0f, 1.5f}},
        fg::LightComponent {.type      = fra::LightType::Point,
                            .color     = {0.85f, 0.9f, 1.0f},
                            .radius    = 20.0f,
                            .intensity = 18.0f});
    fg::TransformUtil::SetParent(*mRegistry, fillLight, mPreviewRoot, true);

    const auto rimLight = mRegistry->CreateEntity(
        fg::NameComponent {.name = "__ShadingPreviewRimLight"},
        fg::TransformComponent {.position = {0.0f, 2.5f, -3.0f}},
        fg::LightComponent {.type      = fra::LightType::Point,
                            .color     = {1.0f, 0.95f, 0.85f},
                            .radius    = 20.0f,
                            .intensity = 25.0f});
    fg::TransformUtil::SetParent(*mRegistry, rimLight, mPreviewRoot, true);

    mPreviewBuilt = true;
}

void ShadingPreviewLayer::syncPreviewMaterial()
{
    if(!mPreviewBuilt || mPreviewSphere == static_cast<fr::Entity>(-1))
    {
        return;
    }

    const auto materialId = mMaterialSelection->HasSelection()
                                ? mMaterialSelection->Get()
                                : mPrimitives->GetDefaultMaterial();

    mRegistry->TryGetComponents<fg::MaterialComponent>(
        mPreviewSphere, [&](fg::MaterialComponent &material) {
            material.materialId = materialId;
        });
}

void ShadingPreviewLayer::syncCamera()
{
    const float yaw   = glm::radians(mYawDegrees);
    const float pitch = glm::radians(mPitchDegrees);
    const glm::vec3 offset {std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                            std::cos(pitch) * std::cos(yaw)};

    auto &camera              = mScene->GetPreviewCamera();
    camera.transform.position = mPivot + offset * mDistance;
    const glm::vec3 toTarget  = glm::normalize(mPivot - camera.transform.position);
    const glm::vec3 worldUp {0.0f, 1.0f, 0.0f};
    glm::vec3 right           = glm::cross(toTarget, worldUp);
    if(glm::dot(right, right) < 1.0e-8f)
    {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    right                     = glm::normalize(right);
    const glm::vec3 up        = glm::normalize(glm::cross(right, toTarget));
    camera.transform.rotation = glm::quatLookAt(toTarget, up);
    camera.fovDegrees         = mFovDegrees;
    camera.nearPlane          = std::max(mMinDistance * 0.01f, 0.01f);
    camera.farPlane           = std::max(mMaxDistance * 4.0f, camera.nearPlane + 10.0f);
}

void ShadingPreviewLayer::handleOrbit()
{
    if(!mViewportHovered)
    {
        return;
    }

    const ImGuiIO &io = ImGui::GetIO();
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
       ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        mYawDegrees += io.MouseDelta.x * mOrbitSensitivity;
        mPitchDegrees =
            std::clamp(mPitchDegrees + io.MouseDelta.y * mOrbitSensitivity, -kPitchLimitDegrees,
                       kPitchLimitDegrees);
    }

    if(std::abs(io.MouseWheel) > 0.0f)
    {
        mDistance =
            std::clamp(mDistance * (1.0f - io.MouseWheel * 0.1f), mMinDistance, mMaxDistance);
    }
}

void ShadingPreviewLayer::onUpdate()
{
    if(mSimulation && mSimulation->IsPlaying())
    {
        mClaimOutput     = false;
        mViewportHovered = false;
        mScene->ClearRenderIsolation();
        return;
    }

    if(!mClaimOutput)
    {
        mScene->ClearRenderIsolation();
        return;
    }

    ensurePreviewScene();
    syncPreviewMaterial();
    mScene->PreferPreviewCamera();
    mScene->SetRenderIsolation(mPreviewRoot);
    syncCamera();

    if(mPreferences &&
       EditorViewport::ApplyQualityPreferences(*mRenderer,
                                               mPreferences->graphics.editorViewport))
    {
        fg::GuiLayer::RecreateMainPipeline(mRenderer);
    }
}

void ShadingPreviewLayer::onGuiBegin()
{
    mPreviewWindowBegun = false;
    if(mSimulation && mSimulation->IsPlaying())
    {
        mClaimOutput       = false;
        mPreviewWindowOpen = false;
        mViewportHovered   = false;
        EditorViewportHost::Request({&mViewport, 0, 0, false});
        return;
    }

    const auto title = EditorDock::WindowId("Preview");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    mPreviewWindowOpen = ImGui::Begin(title.c_str());
    mPreviewWindowBegun = true;
    if(mPreviewWindowOpen)
    {
        mClaimOutput = true;

        mLayoutAvail = ImGui::GetContentRegionAvail();
        EditorViewport::ContentSizeToRenderPixels(mLayoutAvail, mPendingWidth, mPendingHeight);

        EditorViewportHost::Request({&mViewport, mPendingWidth, mPendingHeight, mClaimOutput});
    }
    else
    {
        mClaimOutput     = false;
        mViewportHovered = false;
        EditorViewportHost::Request({&mViewport, 0, 0, false});
    }
}

void ShadingPreviewLayer::onGuiEnd()
{
    if(!mPreviewWindowBegun)
    {
        return;
    }

    if(mPreviewWindowOpen && (!mSimulation || !mSimulation->IsPlaying()))
    {
        if(mViewport.IsActive())
        {
            mViewport.present(mLayoutAvail);
            mViewportHovered = ImGui::IsItemHovered();
            handleOrbit();
        }
        else
        {
            mViewportHovered = false;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    mPreviewWindowOpen = false;
    mPreviewWindowBegun = false;
}
