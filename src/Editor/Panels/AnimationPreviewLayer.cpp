#include "AnimationPreviewLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/ViewportDpi.hpp"
#include "Editor/ViewportQuality.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Gui/Backends/imgui_impl_vulkan.h"
#include "Frigga/Gui/GuiLayer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <imgui.h>
#include <limits>

namespace
{
    constexpr float kPitchLimitDegrees = 85.0f;
    constexpr float kDefaultFovDegrees = 40.0f;
    constexpr float kFitPadding        = 1.25f;
}

AnimationPreviewLayer::AnimationPreviewLayer(skr::Arc<fra::Renderer> renderer,
                                             skr::Arc<fr::Registry> registry,
                                             skr::Arc<fra::MeshPool> meshPool,
                                             skr::Arc<SelectionContext> selection,
                                             skr::Arc<fg::Scene> scene,
                                             skr::Arc<EditorPreferences> preferences)
    : fg::Layer("Preview"), mRenderer(std::move(renderer)), mRegistry(std::move(registry)),
      mMeshPool(std::move(meshPool)), mSelection(std::move(selection)), mScene(std::move(scene)),
      mPreferences(std::move(preferences)),
      mViewport(mRenderer)
{
}

void AnimationPreviewLayer::onAttach()
{
}

void AnimationPreviewLayer::onDettach()
{
    mViewport.Release();
    mScene->ClearRenderIsolation();
}

void AnimationPreviewLayer::onUpdate()
{
    if(!mClaimOutput)
    {
        mScene->ClearRenderIsolation();
        mViewport.Release();
        return;
    }

    mScene->PreferPreviewCamera();

    if(mPreferences &&
       EditorViewport::ApplyQualityPreferences(*mRenderer,
                                               mPreferences->graphics.editorViewport))
    {
        fg::GuiLayer::RecreateMainPipeline(mRenderer);
    }

    if(mSelection->HasSelection())
    {
        mScene->SetRenderIsolation(mSelection->Get());
    }
    else
    {
        mScene->ClearRenderIsolation();
    }

    syncCameraToSelection();
    mViewport.Claim(mPendingWidth, mPendingHeight);
}

void AnimationPreviewLayer::onGui()
{
    const auto title = EditorDock::WindowId("Preview");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if(ImGui::Begin(title.c_str()))
    {
        // Keep live while Timeline / Animator have keyboard focus.
        mClaimOutput = true;

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        EditorViewport::ContentSizeToRenderPixels(avail, mPendingWidth, mPendingHeight);

        if(!mSelection->HasSelection())
        {
            const char *hint = "Select an entity to preview";
            const ImVec2 textSize = ImGui::CalcTextSize(hint);
            ImGui::SetCursorPosX((avail.x - textSize.x) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f);
            ImGui::TextDisabled("%s", hint);
            mViewportHovered = false;
        }
        mViewport.present(avail);
        if(mViewport.IsActive())
        {
            mViewportHovered = ImGui::IsItemHovered();
            handleOrbit();
        }
        else
        {
            mViewportHovered = false;
        }
    }
    else
    {
        mClaimOutput     = false;
        mViewportHovered = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

AnimationPreviewLayer::FrameBounds AnimationPreviewLayer::computeSelectionBounds(
    fr::Entity entity, const fg::TransformComponent &transform) const
{
    FrameBounds bounds {.center = transform.position, .radius = 1.0f};

    std::uint32_t meshId = 0;
    bool hasMesh         = false;
    mRegistry->TryGetComponents<fg::MeshComponent>(entity, [&](fg::MeshComponent &mesh) {
        meshId  = mesh.meshId;
        hasMesh = true;
    });

    if(!hasMesh || mMeshPool == nullptr || !mMeshPool->Contains(meshId))
    {
        const float extent = std::max(
            {std::abs(transform.scale.x), std::abs(transform.scale.y), std::abs(transform.scale.z),
             0.25f});
        bounds.radius = extent * 0.5f;
        return bounds;
    }

    const auto &mesh = mMeshPool->GetMesh(meshId);
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position) *
                            glm::mat4_cast(transform.rotation) * glm::scale(glm::mat4(1.0f), transform.scale);

    const glm::vec3 corners[8] = {
        {mesh.aabbMin.x, mesh.aabbMin.y, mesh.aabbMin.z},
        {mesh.aabbMax.x, mesh.aabbMin.y, mesh.aabbMin.z},
        {mesh.aabbMin.x, mesh.aabbMax.y, mesh.aabbMin.z},
        {mesh.aabbMax.x, mesh.aabbMax.y, mesh.aabbMin.z},
        {mesh.aabbMin.x, mesh.aabbMin.y, mesh.aabbMax.z},
        {mesh.aabbMax.x, mesh.aabbMin.y, mesh.aabbMax.z},
        {mesh.aabbMin.x, mesh.aabbMax.y, mesh.aabbMax.z},
        {mesh.aabbMax.x, mesh.aabbMax.y, mesh.aabbMax.z},
    };

    glm::vec3 worldMin(std::numeric_limits<float>::max());
    glm::vec3 worldMax(std::numeric_limits<float>::lowest());
    for(const glm::vec3 &local : corners)
    {
        const glm::vec3 world = glm::vec3(model * glm::vec4(local, 1.0f));
        worldMin              = glm::min(worldMin, world);
        worldMax              = glm::max(worldMax, world);
    }

    bounds.center = (worldMin + worldMax) * 0.5f;
    bounds.radius = std::max(glm::length(worldMax - worldMin) * 0.5f, 0.05f);
    return bounds;
}

void AnimationPreviewLayer::applyFrame(const FrameBounds &bounds, bool resetOrbit)
{
    mPivot = bounds.center;

    const float halfFov = glm::radians(mFovDegrees * 0.5f);
    const float fitDistance =
        (bounds.radius * kFitPadding) / std::max(std::tan(halfFov), 1.0e-3f);

    mMinDistance = std::max(bounds.radius * 0.35f, 0.05f);
    mMaxDistance = std::max(bounds.radius * 20.0f, mMinDistance * 8.0f);

    if(resetOrbit)
    {
        mDistance     = std::clamp(fitDistance, mMinDistance, mMaxDistance);
        mYawDegrees   = 35.0f;
        mPitchDegrees = 18.0f;
    }
    else
    {
        mDistance = std::clamp(mDistance, mMinDistance, mMaxDistance);
    }
}

void AnimationPreviewLayer::syncCameraToSelection()
{
    if(!mSelection->HasSelection())
    {
        mFramedEntity = SelectionContext::Invalid;
        return;
    }

    const fr::Entity entity = mSelection->Get();
    bool hasTransform       = false;
    fg::TransformComponent transform {};

    mRegistry->TryGetComponents<fg::TransformComponent>(entity, [&](fg::TransformComponent &t) {
        transform    = t;
        hasTransform = true;
    });

    if(!hasTransform)
    {
        return;
    }

    const FrameBounds bounds = computeSelectionBounds(entity, transform);
    applyFrame(bounds, entity != mFramedEntity);
    mFramedEntity = entity;

    const float yaw   = glm::radians(mYawDegrees);
    const float pitch = glm::radians(mPitchDegrees);
    // Orbit on unit sphere: yaw around Y, pitch elevation (positive = above).
    const glm::vec3 offset {std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                            std::cos(pitch) * std::cos(yaw)};

    auto &camera              = mScene->GetPreviewCamera();
    camera.transform.position = mPivot + offset * mDistance;
    // Freya camera looks along local -Z; quatLookAt(dir) aims -Z toward dir.
    const glm::vec3 toTarget = glm::normalize(mPivot - camera.transform.position);
    const glm::vec3 worldUp {0.0f, 1.0f, 0.0f};
    glm::vec3 right = glm::cross(toTarget, worldUp);
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

void AnimationPreviewLayer::handleOrbit()
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
