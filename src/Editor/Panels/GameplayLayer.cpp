#include "GameplayLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"

#include <Frigga/Gui/Backends/imgui_impl_vulkan.h>
#include <Frigga/Physics/ColliderDebugDraw.hpp>

#include <algorithm>
#include <cstdint>
#include <imgui.h>

GameplayLayer::GameplayLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                             skr::Arc<fg::Scene> scene,
                             skr::Arc<fg::PrimitiveMeshFactory> primitives,
                             skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Gameplay"), mRenderer(std::move(renderer)), mRegistry(std::move(registry)),
      mScene(std::move(scene)), mPrimitives(std::move(primitives)),
      mSimulation(std::move(simulation))
{
}

void GameplayLayer::onAttach()
{
    // Editor owns the output target by default; prepare a dormant target size only.
}

void GameplayLayer::onDettach()
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

void GameplayLayer::onUpdate()
{
    if(!mSimulation->IsPlaying())
    {
        mClaimOutput = false;
        return;
    }

    mClaimOutput = true;
    mScene->PreferGameplayCamera();
    ensureTarget(mPendingWidth, mPendingHeight);
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

    const auto &projectionUbo = mRenderer->GetCurrentProjection();
    fg::ColliderDebugDraw::Draw(ImGui::GetWindowDrawList(), mRegistry, mPrimitives,
                                projectionUbo.view, projectionUbo.projection, imageMin, imageSize);
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

        drawToolbar();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        mPendingWidth      = static_cast<std::uint32_t>(std::max(avail.x, 1.0f));
        mPendingHeight     = static_cast<std::uint32_t>(std::max(avail.y, 1.0f));

        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        if(mTextureId != VK_NULL_HANDLE)
        {
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mTextureId)),
                         avail);
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

void GameplayLayer::ensureTarget(std::uint32_t width, std::uint32_t height)
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

void GameplayLayer::releaseTexture()
{
    if(mTextureId != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(mTextureId);
        mTextureId = VK_NULL_HANDLE;
    }
}

void GameplayLayer::recreateUiPipeline()
{
    ImGui_ImplVulkan_PipelineInfo pipelineInfo {};
    pipelineInfo.RenderPass = mRenderer->GetUIRenderPass();
    ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
}
