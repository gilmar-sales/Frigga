#include "EditorLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Editor/EditorViewportHost.hpp"
#include "Editor/UiScale.hpp"
#include "Editor/ViewportDpi.hpp"
#include "Editor/ViewportQuality.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Gui/Backends/imgui_impl_vulkan.h"
#include "Frigga/Gui/GuiLayer.hpp"
#include "Frigga/Physics/ColliderDebugDraw.hpp"
#include "Frigga/Editor/LightDebugDraw.hpp"
#include "Frigga/Editor/CameraDebugDraw.hpp"
#include "Frigga/Editor/AudioDebugDraw.hpp"
#include "Frigga/Editor/InfiniteGridDraw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

namespace
{
    constexpr float kPitchLimitDegrees     = 89.0f;
    constexpr float kMinOrbitDistance      = 0.25f;
    constexpr float kDefaultEditorFovDegrees = 50.0f;
    constexpr float kMinFovDegrees           = 1.0f;
    constexpr float kMaxFovDegrees           = 179.0f;
    constexpr float kFovZoomDegreesPerSecond = 5.0f;
}

EditorLayer::EditorLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                         skr::Arc<fg::PrimitiveMeshFactory> primitives,
                         skr::Arc<SelectionContext> selection, skr::Arc<fg::Scene> scene,
                         skr::Arc<fg::SceneSimulationState> simulation,
                         skr::Arc<fg::UserComponentRegistry> userComponents,
                         skr::Arc<EditorPreferences> preferences)
    : fg::Layer("Editor"), mRenderer(std::move(renderer)), mRegistry(std::move(registry)),
      mPrimitives(std::move(primitives)), mSelection(std::move(selection)),
      mScene(std::move(scene)), mSimulation(std::move(simulation)),
      mUserComponents(std::move(userComponents)),
      mPreferences(std::move(preferences)),
      mViewport(mRenderer)
{
}

void EditorLayer::onAttach()
{
}

void EditorLayer::onDettach()
{
    mViewport.Release();
    mWidth  = 0;
    mHeight = 0;
}

void EditorLayer::onSuspend()
{
    mClaimOutput     = false;
    mViewportHovered = false;
    mViewportFocused = false;
    mNavMode         = NavMode::None;
    mViewport.Suspend();
    mWidth  = 0;
    mHeight = 0;
}

void EditorLayer::onUpdate()
{
    consumePickResult();

    if(mSimulation->IsPlaying())
    {
        mClaimOutput     = false;
        mViewportHovered = false;
        mViewportFocused = false;
        mNavMode         = NavMode::None;
        return;
    }

    if(mPreferences &&
       EditorViewport::ApplyQualityPreferences(*mRenderer,
                                               mPreferences->graphics.editorViewport))
    {
        fg::GuiLayer::RecreateMainPipeline(mRenderer);
    }

    mScene->PreferEditorCamera();
}

void EditorLayer::onGuiBegin()
{
    ImGuizmo::BeginFrame();
    mEditorWindowBegun = false;

    if(mSimulation->IsPlaying())
    {
        mClaimOutput       = false;
        mEditorWindowOpen  = false;
        mViewportHovered   = false;
        mViewportFocused   = false;
        mNavMode           = NavMode::None;
        EditorViewportHost::Request({&mViewport, 0, 0, false});
        return;
    }

    const auto title = EditorDock::WindowId("Editor");

    if(mSimulation->ConsumeFocusEditorRequest())
    {
        ImGui::SetNextWindowFocus();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    mEditorWindowOpen = ImGui::Begin(title.c_str());
    mEditorWindowBegun = true;
    if(mEditorWindowOpen)
    {
        mViewportFocused =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        mClaimOutput = true;

        drawToolbar();

        mLayoutAvail = ImGui::GetContentRegionAvail();
        EditorViewport::ContentSizeToRenderPixels(mLayoutAvail, mPendingWidth, mPendingHeight);
        mLayoutImageMin = ImGui::GetCursorScreenPos();

        EditorViewportHost::Request({&mViewport, mPendingWidth, mPendingHeight,
                                     mClaimOutput && !mSimulation->IsPlaying()});
    }
    else
    {
        mClaimOutput     = false;
        mViewportHovered = false;
        mViewportFocused = false;
        mNavMode         = NavMode::None;
        EditorViewportHost::Request({&mViewport, 0, 0, false});
    }
}

void EditorLayer::onGuiEnd()
{
    if(!mEditorWindowBegun)
    {
        return;
    }

    if(mEditorWindowOpen && !mSimulation->IsPlaying())
    {
        mWidth  = mPendingWidth;
        mHeight = mPendingHeight;

        if(mViewport.IsActive())
        {
            mViewport.present(mLayoutAvail);
            mViewportHovered = ImGui::IsItemHovered();
            handleNavigation();

            glm::mat4 view;
            glm::mat4 projection;
            const bool hasCamera = computeActiveCamera(view, projection);

            const bool navigating = mNavMode != NavMode::None;
            ImGuizmo::Enable(!navigating);
            drawGizmos(mLayoutImageMin, mLayoutAvail, !navigating);
            if(hasCamera)
            {
                const fr::Entity selected = mSelection->HasSelection() ? mSelection->Get()
                                                                       : SelectionContext::Invalid;
                fg::LightDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, view, projection,
                                         mLayoutImageMin, mLayoutAvail, selected);
                fg::CameraDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, view, projection,
                                          mLayoutImageMin, mLayoutAvail, selected,
                                          mScene->GetMainCameraEntity());
                fg::AudioDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, view, projection,
                                         mLayoutImageMin, mLayoutAvail, selected);
                if(mSimulation->GetShowColliders())
                {
                    fg::ColliderDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, mPrimitives,
                                                view, projection, mLayoutImageMin, mLayoutAvail,
                                                selected, {}, false, mUserComponents);
                }
            }
            handlePicking(mLayoutImageMin, mLayoutAvail);
        }
        else
        {
            mViewportHovered = false;
            mNavMode         = NavMode::None;
        }

        if((mViewportHovered || mViewportFocused) && mNavMode == NavMode::None &&
           !ImGuizmo::IsUsing())
        {
            if(ImGui::IsKeyPressed(ImGuiKey_W))
            {
                mOperation = ImGuizmo::TRANSLATE;
            }
            else if(ImGui::IsKeyPressed(ImGuiKey_E))
            {
                mOperation = ImGuizmo::ROTATE;
            }
            else if(ImGui::IsKeyPressed(ImGuiKey_R))
            {
                mOperation = ImGuizmo::SCALE;
            }
            else if(ImGui::IsKeyPressed(ImGuiKey_F))
            {
                mScene->GetEditorCamera().fovDegrees = kDefaultEditorFovDegrees;
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    mEditorWindowOpen = false;
    mEditorWindowBegun = false;
}

void EditorLayer::drawToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorUiScale::V(8.0f, 6.0f));
    ImGui::SetCursorPos(
        ImVec2(EditorUiScale::S(8.0f), ImGui::GetCursorPosY() + EditorUiScale::S(6.0f)));

    if(ImGui::Button(ICON_BTSP_PLAY " Play"))
    {
        mSimulation->Play();
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Play simulation (Ctrl+P)");
    }

    ImGui::SameLine();
    bool showColliders = mSimulation->GetShowColliders();
    if(ImGui::Checkbox(ICON_BTSP_BOUNDINGBOX " Colliders", &showColliders))
    {
        mSimulation->SetShowColliders(showColliders);
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if(ImGui::RadioButton("Translate (W)", mOperation == ImGuizmo::TRANSLATE))
    {
        mOperation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if(ImGui::RadioButton("Rotate (E)", mOperation == ImGuizmo::ROTATE))
    {
        mOperation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if(ImGui::RadioButton("Scale (R)", mOperation == ImGuizmo::SCALE))
    {
        mOperation = ImGuizmo::SCALE;
    }

    if(mOperation != ImGuizmo::SCALE)
    {
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        if(ImGui::RadioButton("Local", mMode == ImGuizmo::LOCAL))
        {
            mMode = ImGuizmo::LOCAL;
        }
        ImGui::SameLine();
        if(ImGui::RadioButton("World", mMode == ImGuizmo::WORLD))
        {
            mMode = ImGuizmo::WORLD;
        }
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &mDrawGrid);

    ImGui::PopStyleVar();
    ImGui::Dummy(EditorUiScale::V(0.0f, 4.0f));
}

void EditorLayer::consumePickResult()
{
    std::uint32_t id = 0;
    if(!mRenderer->TryConsumePickResult(id))
    {
        return;
    }

    if(id == SelectionContext::Invalid)
    {
        mSelection->Clear();
    }
    else
    {
        mSelection->Select(static_cast<fr::Entity>(id));
    }
}

void EditorLayer::handlePicking(const ImVec2 &imageMin, const ImVec2 &imageSize)
{
    ImGuiIO &io = ImGui::GetIO();

    if(!mViewportHovered || mNavMode != NavMode::None || io.KeyAlt ||
       ImGuizmo::IsUsing() || ImGuizmo::IsOver() ||
       !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        return;
    }

    if(mWidth == 0 || mHeight == 0 || imageSize.x <= 0.0f || imageSize.y <= 0.0f)
    {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const float u      = (mouse.x - imageMin.x) / imageSize.x;
    const float v      = (mouse.y - imageMin.y) / imageSize.y;
    if(u < 0.0f || v < 0.0f || u >= 1.0f || v >= 1.0f)
    {
        return;
    }

    // Prefer light/camera gizmos (screen-space) over GPU mesh pick.
    glm::mat4 view;
    glm::mat4 projection;
    if(computeActiveCamera(view, projection))
    {
        if(const auto cameraHit = fg::CameraDebugDraw::HitTest(
               mRegistry, view, projection, imageMin, imageSize, mouse))
        {
            mSelection->Select(*cameraHit);
            return;
        }
        if(const auto audioHit = fg::AudioDebugDraw::HitTest(
               mRegistry, view, projection, imageMin, imageSize, mouse))
        {
            mSelection->Select(*audioHit);
            return;
        }
        if(const auto lightHit = fg::LightDebugDraw::HitTest(
               mRegistry, view, projection, imageMin, imageSize, mouse))
        {
            mSelection->Select(*lightHit);
            return;
        }
    }

    const auto x = static_cast<std::uint32_t>(u * static_cast<float>(mWidth));
    const auto y = static_cast<std::uint32_t>(v * static_cast<float>(mHeight));
    mRenderer->RequestPick(std::min(x, mWidth - 1), std::min(y, mHeight - 1));
}

void EditorLayer::handleNavigation()
{
    fg::TransformComponent &camera = mScene->GetEditorCamera().transform;
    ImGuiIO &io                    = ImGui::GetIO();
    const float dt                 = std::max(io.DeltaTime, 0.0f);

    const bool allowStart =
        mViewportHovered && !ImGuizmo::IsUsing() && mNavMode == NavMode::None;

    if(allowStart)
    {
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            mNavMode = NavMode::Fly;
            extractYawPitch(camera, mFlyYawDegrees, mFlyPitchDegrees);
        }
        else if(io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            mNavMode = NavMode::Orbit;
            syncOrbitPivot(camera);
        }
        else if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        {
            mNavMode = NavMode::Pan;
            syncOrbitPivot(camera);
        }
    }

    if(mNavMode == NavMode::Fly && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        mNavMode = NavMode::None;
    }
    else if(mNavMode == NavMode::Orbit &&
            !(io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)))
    {
        mNavMode = NavMode::None;
    }
    else if(mNavMode == NavMode::Pan && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        mNavMode = NavMode::None;
    }

    // Scroll zooms by adjusting the editor camera FOV (position stays put).
    if((mViewportHovered || mNavMode != NavMode::None) && io.MouseWheel != 0.0f &&
       !ImGuizmo::IsUsing())
    {
        auto &editorCamera = mScene->GetEditorCamera();
        // Scroll up zooms in (narrower FOV), rate in degrees/second.
        editorCamera.fovDegrees = std::clamp(
            editorCamera.fovDegrees - io.MouseWheel * kFovZoomDegreesPerSecond * dt,
            kMinFovDegrees, kMaxFovDegrees);
    }

    if(mNavMode == NavMode::None)
    {
        return;
    }

    const ImVec2 mouseDelta = io.MouseDelta;

    if(mNavMode == NavMode::Fly)
    {
        mFlyYawDegrees += mouseDelta.x * mLookSensitivity;
        mFlyPitchDegrees -= mouseDelta.y * mLookSensitivity;
        mFlyPitchDegrees =
            std::clamp(mFlyPitchDegrees, -kPitchLimitDegrees, kPitchLimitDegrees);
        applyYawPitch(camera, mFlyYawDegrees, mFlyPitchDegrees);

        const float boost       = io.KeyShift ? 3.0f : 1.0f;
        const float speed       = mMoveSpeed * boost * dt;
        const glm::vec3 forward = cameraForward(camera);
        const glm::vec3 right   = cameraRight(camera);
        const glm::vec3 up {0.0f, 1.0f, 0.0f};

        glm::vec3 move {0.0f};
        if(ImGui::IsKeyDown(ImGuiKey_W))
        {
            move += forward;
        }
        if(ImGui::IsKeyDown(ImGuiKey_S))
        {
            move -= forward;
        }
        if(ImGui::IsKeyDown(ImGuiKey_D))
        {
            move += right;
        }
        if(ImGui::IsKeyDown(ImGuiKey_A))
        {
            move -= right;
        }
        if(ImGui::IsKeyDown(ImGuiKey_E))
        {
            move += up;
        }
        if(ImGui::IsKeyDown(ImGuiKey_Q))
        {
            move -= up;
        }

        if(glm::dot(move, move) > 1e-6f)
        {
            camera.position += glm::normalize(move) * speed;
        }

        mOrbitPivot = camera.position + cameraForward(camera) * mOrbitDistance;
    }
    else if(mNavMode == NavMode::Orbit)
    {
        float yaw   = 0.0f;
        float pitch = 0.0f;
        extractYawPitch(camera, yaw, pitch);

        yaw += mouseDelta.x * mOrbitSensitivity;
        pitch -= mouseDelta.y * mOrbitSensitivity;
        pitch = std::clamp(pitch, -kPitchLimitDegrees, kPitchLimitDegrees);
        applyYawPitch(camera, yaw, pitch);

        camera.position = mOrbitPivot - cameraForward(camera) * mOrbitDistance;
    }
    else if(mNavMode == NavMode::Pan)
    {
        const float panScale  = std::max(mOrbitDistance, 1.0f) * 0.0025f;
        const glm::vec3 right = cameraRight(camera);
        const glm::vec3 up    = cameraUp(camera);
        const glm::vec3 delta =
            (-right * mouseDelta.x + up * mouseDelta.y) * panScale;
        camera.position += delta;
        mOrbitPivot += delta;
    }
}

void EditorLayer::syncOrbitPivot(const fg::TransformComponent &camera)
{
    if(mSelection->HasSelection())
    {
        bool usedSelection = false;
        mRegistry->TryGetComponents<fg::TransformComponent>(
            mSelection->Get(), [&](fg::TransformComponent &selected) {
                mOrbitPivot    = selected.position;
                mOrbitDistance = std::max(kMinOrbitDistance,
                                          glm::length(camera.position - mOrbitPivot));
                usedSelection  = true;
            });
        if(usedSelection)
        {
            return;
        }
    }

    const glm::vec3 forward = cameraForward(camera);
    mOrbitPivot             = camera.position + forward * mOrbitDistance;
}

void EditorLayer::applyYawPitch(fg::TransformComponent &camera, float yawDegrees,
                                float pitchDegrees) const
{
    const float yaw   = glm::radians(yawDegrees);
    const float pitch = glm::radians(pitchDegrees);

    const glm::vec3 forward {
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        -std::cos(yaw) * std::cos(pitch),
    };

    glm::vec3 safeForward = forward;
    if(glm::dot(safeForward, safeForward) < 1e-6f)
    {
        safeForward = {0.0f, 0.0f, -1.0f};
    }
    else
    {
        safeForward = glm::normalize(safeForward);
    }

    camera.rotation = glm::quatLookAt(safeForward, glm::vec3 {0.0f, 1.0f, 0.0f});
}

void EditorLayer::extractYawPitch(const fg::TransformComponent &camera, float &yawDegrees,
                                  float &pitchDegrees) const
{
    const glm::vec3 forward = cameraForward(camera);
    pitchDegrees =
        glm::degrees(std::asin(std::clamp(forward.y, -1.0f, 1.0f)));
    yawDegrees = glm::degrees(std::atan2(forward.x, -forward.z));
}

glm::vec3 EditorLayer::cameraForward(const fg::TransformComponent &camera)
{
    const glm::vec3 forward =
        glm::normalize(camera.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    if(glm::dot(forward, forward) < 1e-6f)
    {
        return {0.0f, 0.0f, -1.0f};
    }
    return forward;
}

glm::vec3 EditorLayer::cameraRight(const fg::TransformComponent &camera)
{
    const glm::vec3 right =
        glm::normalize(camera.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
    if(glm::dot(right, right) < 1e-6f)
    {
        return {1.0f, 0.0f, 0.0f};
    }
    return right;
}

glm::vec3 EditorLayer::cameraUp(const fg::TransformComponent &camera)
{
    const glm::vec3 up = glm::normalize(camera.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
    if(glm::dot(up, up) < 1e-6f)
    {
        return {0.0f, 1.0f, 0.0f};
    }
    return up;
}

void EditorLayer::drawGizmos(const ImVec2 &imageMin, const ImVec2 &imageSize, bool allowManipulate)
{
    if(imageSize.x < 1.0f || imageSize.y < 1.0f)
    {
        return;
    }

    glm::mat4 viewVulkan;
    glm::mat4 projectionVulkan;
    if(!computeActiveCamera(viewVulkan, projectionVulkan))
    {
        return;
    }
    // Match Freya view; undo Vulkan Y-flip so ImGuizmo's OpenGL NDC mapping lines up
    // with the ImGui image (see ImGuizmo::worldToPos `trans.y = 1.f - trans.y`).
    const glm::mat4 view = viewVulkan;
    const glm::mat4 proj = gizmoProjection(projectionVulkan);

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);

    if(mDrawGrid)
    {
        // Infinite Y=0 grid clipped to the visible ground region (soft contrast).
        fg::InfiniteGridDraw::Draw(ImGui::GetWindowDrawList(), view, projectionVulkan,
                                   imageMin, imageSize);
    }

    if(!allowManipulate || !mSelection->HasSelection())
    {
        return;
    }

    const fr::Entity selected = mSelection->Get();
    if(!mRegistry->HasComponent<fg::TransformComponent>(selected))
    {
        return;
    }

    mRegistry->TryGetComponents<fg::TransformComponent>(
        selected, [this, &view, &proj, selected](fg::TransformComponent &) {
            glm::mat4 model = fg::TransformUtil::WorldMatrix(*mRegistry, selected);

            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), mOperation, mMode,
                                 glm::value_ptr(model));

            if(ImGuizmo::IsUsing())
            {
                fg::TransformUtil::SetWorldMatrix(*mRegistry, selected, model);
            }
        });
}

glm::mat4 EditorLayer::gizmoProjection(const glm::mat4 &vulkanProjection)
{
    glm::mat4 projection = vulkanProjection;
    projection[1][1] *= -1.0f;
    return projection;
}

bool EditorLayer::computeActiveCamera(glm::mat4 &viewOut, glm::mat4 &projectionOut) const
{
    const fg::EditorCamera &camera =
        mScene->IsUsingPreviewCamera() ? mScene->GetPreviewCamera() : mScene->GetEditorCamera();

    const float aspect = mHeight > 0 ? static_cast<float>(mWidth) / static_cast<float>(mHeight)
                                     : 16.0f / 9.0f;

    const auto matrices = fg::ViewportTarget::Compute(
        mRenderer, camera.transform.position, camera.transform.rotation, camera.fovDegrees,
        camera.nearPlane, camera.farPlane, aspect);
    viewOut       = matrices.view;
    projectionOut = matrices.projection;
    return true;
}
