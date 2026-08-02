#include "GameplayLayer.hpp"

#include <Frigga/Gui/Backends/imgui_impl_vulkan.h>

#include <algorithm>
#include <cstdint>
#include <imgui.h>

GameplayLayer::GameplayLayer(skr::Arc<fra::Renderer> renderer)
    : fg::Layer("Gameplay"), mRenderer(std::move(renderer))
{
}

void GameplayLayer::onAttach()
{
    ensureTarget(mPendingWidth, mPendingHeight);
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
    ensureTarget(mPendingWidth, mPendingHeight);
}

void GameplayLayer::onGui()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if(ImGui::Begin("Gameplay"))
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        mPendingWidth      = static_cast<std::uint32_t>(std::max(avail.x, 1.0f));
        mPendingHeight     = static_cast<std::uint32_t>(std::max(avail.y, 1.0f));

        if(mTextureId != VK_NULL_HANDLE)
        {
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mTextureId)),
                         avail);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void GameplayLayer::ensureTarget(std::uint32_t width, std::uint32_t height)
{
    if(mTarget && mWidth == width && mHeight == height)
    {
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
