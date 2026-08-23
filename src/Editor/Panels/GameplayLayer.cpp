#include "GameplayLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Editor/ViewportDpi.hpp"
#include "Editor/ViewportQuality.hpp"

#include <Frigga/Gui/Backends/imgui_impl_vulkan.h>
#include <Frigga/Gui/GuiLayer.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Physics/ColliderDebugDraw.hpp>
#include <Frigga/ECS/Components/CameraComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <algorithm>
#include <cstdint>
#include <imgui.h>

GameplayLayer::GameplayLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                             skr::Arc<fg::Scene> scene,
                             skr::Arc<fg::PrimitiveMeshFactory> primitives,
                             skr::Arc<fg::SceneSimulationState> simulation,
                             skr::Arc<SelectionContext> selection,
                             skr::Arc<fg::IPhysicsWorld> physicsWorld,
                             skr::Arc<fg::UserComponentRegistry> userComponents,
                             skr::Arc<EditorPreferences> preferences,
                             skr::Arc<fg::Input> input)
    : fg::Layer("Gameplay"), mRenderer(std::move(renderer)), mRegistry(std::move(registry)),
      mScene(std::move(scene)), mPrimitives(std::move(primitives)),
      mSimulation(std::move(simulation)), mSelection(std::move(selection)),
      mPhysicsWorld(std::move(physicsWorld)), mUserComponents(std::move(userComponents)),
      mPreferences(std::move(preferences)),
      mInput(std::move(input)),
      mViewport(mRenderer)
{
}

void GameplayLayer::onAttach()
{
    // Editor owns the output target by default; prepare a dormant target size only.
}

void GameplayLayer::onDettach()
{
    mViewport.Release();
}

void GameplayLayer::onUpdate()
{
    if(!mSimulation->IsPlaying())
    {
        mClaimOutput = false;
        if(mInput)
        {
            mInput->SetGameplayViewportHovered(false);
        }
        mViewport.Release();
        return;
    }

    mClaimOutput = true;
    if(mPreferences &&
       EditorViewport::ApplyQualityPreferences(*mRenderer,
                                               mPreferences->graphics.gameplayViewport))
    {
        fg::GuiLayer::RecreateMainPipeline(mRenderer);
    }
    mScene->PreferGameplayCamera();
    mViewport.Claim(mPendingWidth, mPendingHeight);
}

void GameplayLayer::drawToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::SetCursorPos(ImVec2(8.0f, ImGui::GetCursorPosY() + 6.0f));

    if(mSimulation->IsPaused())
    {
        if(ImGui::Button(ICON_BTSP_PLAY " Resume"))
        {
            mSimulation->Resume();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Resume simulation (Ctrl+P)");
        }
    }
    else
    {
        if(ImGui::Button(ICON_BTSP_PAUSE " Pause"))
        {
            mSimulation->Pause();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pause simulation (Ctrl+P)");
        }
    }

    ImGui::SameLine();
    if(ImGui::Button(ICON_BTSP_SKIPFORWARD " Step"))
    {
        mSimulation->Step();
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Step one physics tick (Ctrl+.)");
    }

    ImGui::SameLine();
    if(ImGui::Button(ICON_BTSP_SKIPEND " Stop"))
    {
        mSimulation->Stop();
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Stop and restore edit scene (Ctrl+Shift+P)");
    }

    ImGui::SameLine();
    bool showColliders = mSimulation->GetShowColliders();
    if(ImGui::Checkbox(ICON_BTSP_BOUNDINGBOX " Colliders", &showColliders))
    {
        mSimulation->SetShowColliders(showColliders);
    }

    ImGui::SameLine();
    ImGui::TextDisabled(mSimulation->IsPaused() ? "Paused" : "Playing");

    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

void GameplayLayer::drawColliders(const ImVec2 &imageMin, const ImVec2 &imageSize)
{
    if(!mSimulation->GetShowColliders())
    {
        return;
    }

    const fr::Entity camera = mScene->GetMainCameraEntity();
    glm::mat4        view;
    glm::mat4        projection;
    if(!computeActiveCamera(view, projection))
    {
        return;
    }
    (void)camera;

    const fr::Entity selected =
        mSelection->HasSelection() ? mSelection->Get() : SelectionContext::Invalid;
    fg::ColliderDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, mPrimitives, view,
                                projection, imageMin, imageSize, selected, mPhysicsWorld,
                                /*dimInactiveBodies=*/true, mUserComponents);
}

bool GameplayLayer::computeActiveCamera(glm::mat4 &viewOut, glm::mat4 &projectionOut)
{
    const fr::Entity camera = mScene->GetMainCameraEntity();

    glm::vec3 position {0.0f};
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
    float      fovDegrees = 60.0f;
    float      nearPlane  = 0.1f;
    float      farPlane   = 1000.0f;
    bool       found      = false;

    mRegistry->TryGetComponents<fg::TransformComponent>(
        camera, [&](const fg::TransformComponent &transform) {
            const auto pose = fg::TransformUtil::WorldPose(*mRegistry, camera);
            position        = pose.position;
            rotation        = pose.rotation;
            (void)transform;
        });
    mRegistry->TryGetComponents<fg::CameraComponent>(
        camera, [&](const fg::CameraComponent &cam) {
            fovDegrees = cam.fovDegrees;
            nearPlane  = cam.nearPlane;
            farPlane   = cam.farPlane;
            found      = true;
        });
    if(!found)
    {
        return false;
    }

    const float aspect = mPendingHeight > 0
                             ? static_cast<float>(mPendingWidth) /
                                   static_cast<float>(mPendingHeight)
                             : 16.0f / 9.0f;

    const auto matrices = fg::ViewportTarget::Compute(mRenderer, position, rotation, fovDegrees,
                                                      nearPlane, farPlane, aspect);
    viewOut       = matrices.view;
    projectionOut = matrices.projection;
    return true;
}

void GameplayLayer::onGui()
{
    // Exclusive with Editor: Gameplay is only visible while playing.
    if(!mSimulation->IsPlaying())
    {
        mClaimOutput = false;
        return;
    }

    const auto title = EditorDock::WindowId("Gameplay");

    if(mSimulation->ConsumeFocusGameplayRequest())
    {
        ImGui::SetNextWindowFocus();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if(ImGui::Begin(title.c_str()))
    {
        mClaimOutput = true;

        if(mInput)
        {
            mInput->SetGameplayViewportHovered(
                ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup));
        }

        drawToolbar();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        EditorViewport::ContentSizeToRenderPixels(avail, mPendingWidth, mPendingHeight);

        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        mViewport.present(avail);
        if(mViewport.IsActive())
        {
            drawColliders(imageMin, avail);
        }
    }
    else
    {
        mClaimOutput = true;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
