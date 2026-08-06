#include "EditorLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Gui/Backends/imgui_impl_vulkan.h"
#include "Frigga/Physics/ColliderDebugDraw.hpp"
#include "Frigga/Editor/LightDebugDraw.hpp"

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
                         skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Editor"), mRenderer(std::move(renderer)), mRegistry(std::move(registry)),
      mPrimitives(std::move(primitives)), mSelection(std::move(selection)),
      mScene(std::move(scene)), mSimulation(std::move(simulation))
{
}

void EditorLayer::onAttach()
{
    ensureTarget(mPendingWidth, mPendingHeight);
}

void EditorLayer::onDettach()
{
    releaseTexture();
    if(mRenderer->GetOutputTarget() == mTarget)
    {
        mRenderer->ClearOutputTarget();
        recreateUiPipeline();
    }
    mTarget.reset();
    mWidth  = 0;
    mHeight = 0;
}

void EditorLayer::onUpdate()
{
    consumePickResult();

    if(mSimulation->IsPlaying())
    {
        mClaimOutput = false;
        return;
    }

    if(mClaimOutput)
    {
        mScene->PreferEditorCamera();
        ensureTarget(mPendingWidth, mPendingHeight);
    }
}

void EditorLayer::onGui()
{
    ImGuizmo::BeginFrame();

    // Exclusive with Gameplay: Editor is only visible while editing.
    if(mSimulation->IsPlaying())
    {
        mClaimOutput     = false;
        mViewportHovered = false;
        mViewportFocused = false;
        mNavMode         = NavMode::None;
        return;
    }

    const auto title = EditorDock::WindowId("Editor");

    if(mSimulation->ConsumeFocusEditorRequest())
    {
        ImGui::SetNextWindowFocus();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if(ImGui::Begin(title.c_str()))
    {
        mViewportFocused =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        mClaimOutput = mViewportFocused || ImGui::IsWindowHovered();

        drawToolbar();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        mPendingWidth      = static_cast<std::uint32_t>(std::max(avail.x, 1.0f));
        mPendingHeight     = static_cast<std::uint32_t>(std::max(avail.y, 1.0f));

        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        if(mTextureId != VK_NULL_HANDLE)
        {
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mTextureId)),
                         avail);
            mViewportHovered = ImGui::IsItemHovered();
            handleNavigation();

            const bool navigating = mNavMode != NavMode::None;
            ImGuizmo::Enable(!navigating);
            drawGizmos(imageMin, avail, !navigating);
            {
                const auto &projectionUbo = mRenderer->GetCurrentProjection();
                const fr::Entity selected = mSelection->HasSelection()
                                                ? mSelection->Get()
                                                : SelectionContext::Invalid;
                fg::LightDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, projectionUbo.view,
                                         projectionUbo.projection, imageMin, avail, selected);
            }
            if(mSimulation->GetShowColliders())
            {
                const auto &projectionUbo = mRenderer->GetCurrentProjection();
                const fr::Entity selected = mSelection->HasSelection()
                                                ? mSelection->Get()
                                                : SelectionContext::Invalid;
                fg::ColliderDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, mPrimitives,
                                            projectionUbo.view, projectionUbo.projection, imageMin,
                                            avail, selected);
            }
            handlePicking(imageMin, avail);
        }
        else
        {
            mViewportHovered = false;
            mNavMode         = NavMode::None;
        }

        // Tool / view hotkeys only when not flying (RMB holds WASD for movement).
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
    else
    {
        mClaimOutput     = false;
        mViewportHovered = false;
        mViewportFocused = false;
        mNavMode         = NavMode::None;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::drawToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::SetCursorPos(ImVec2(8.0f, ImGui::GetCursorPosY() + 6.0f));

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
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
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

    // Prefer light gizmos (screen-space) over GPU mesh pick.
    {
        const auto &projectionUbo = mRenderer->GetCurrentProjection();
        if(const auto lightHit =
               fg::LightDebugDraw::HitTest(mRegistry, projectionUbo.view, projectionUbo.projection,
                                           imageMin, imageSize, mouse))
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

    const auto &projectionUbo = mRenderer->GetCurrentProjection();
    // Match Freya view; undo Vulkan Y-flip so ImGuizmo's OpenGL NDC mapping lines up
    // with the ImGui image (see ImGuizmo::worldToPos `trans.y = 1.f - trans.y`).
    const glm::mat4 view = projectionUbo.view;
    const glm::mat4 proj = gizmoProjection(projectionUbo.projection);

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);

    if(mDrawGrid)
    {
        const glm::mat4 identity(1.0f);
        ImGuizmo::DrawGrid(glm::value_ptr(view), glm::value_ptr(proj), glm::value_ptr(identity),
                           20.0f);
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
        selected, [this, &view, &proj](fg::TransformComponent &transform) {
            glm::mat4 model = buildModelMatrix(transform);

            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), mOperation, mMode,
                                 glm::value_ptr(model));

            if(ImGuizmo::IsUsing())
            {
                applyModelMatrix(transform, model);
            }
        });
}

glm::mat4 EditorLayer::gizmoProjection(const glm::mat4 &vulkanProjection)
{
    glm::mat4 projection = vulkanProjection;
    projection[1][1] *= -1.0f;
    return projection;
}

glm::mat4 EditorLayer::buildModelMatrix(const fg::TransformComponent &transform)
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
    model           = model * glm::mat4_cast(transform.rotation);
    model           = glm::scale(model, transform.scale);
    return model;
}

void EditorLayer::applyModelMatrix(fg::TransformComponent &transform, const glm::mat4 &matrix)
{
    float translation[3];
    float rotation[3];
    float scale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), translation, rotation, scale);

    transform.position = {translation[0], translation[1], translation[2]};
    transform.scale    = {scale[0], scale[1], scale[2]};
    transform.rotation =
        glm::quat(glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2])));
}

void EditorLayer::ensureTarget(std::uint32_t width, std::uint32_t height)
{
    if(mTarget && mWidth == width && mHeight == height)
    {
        if(mRenderer->GetOutputTarget() != mTarget)
        {
            mRenderer->SetOutputTarget(mTarget);
            recreateUiPipeline();
        }
        return;
    }

    releaseTexture();

    mTarget = mRenderer->GetRenderTargetBuilder().SetWidth(width).SetHeight(height).Build();
    mRenderer->SetOutputTarget(mTarget);
    recreateUiPipeline();

    mTextureId = ImGui_ImplVulkan_AddTexture(
        mTarget->GetSampler(), mTarget->GetColorImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    mWidth  = width;
    mHeight = height;
}

void EditorLayer::releaseTexture()
{
    if(mTextureId != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(mTextureId);
        mTextureId = VK_NULL_HANDLE;
    }
}

void EditorLayer::recreateUiPipeline()
{
    ImGui_ImplVulkan_PipelineInfo pipelineInfo {};
    pipelineInfo.RenderPass = mRenderer->GetUIRenderPass();
    ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
}
