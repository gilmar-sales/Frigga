#include "EditorLayer.hpp"

#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Gui/Backends/imgui_impl_vulkan.h"

#include <algorithm>
#include <cstdint>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

EditorLayer::EditorLayer(skr::Arc<fra::Renderer> renderer, skr::Arc<fr::Registry> registry,
                         skr::Arc<SelectionContext> selection)
    : fg::Layer("Editor"), mRenderer(std::move(renderer)), mRegistry(std::move(registry)),
      mSelection(std::move(selection))
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
    if(mClaimOutput)
    {
        ensureTarget(mPendingWidth, mPendingHeight);
    }
}

void EditorLayer::onGui()
{
    ImGuizmo::BeginFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if(ImGui::Begin("Editor"))
    {
        mClaimOutput =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsWindowHovered();

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
            drawGizmos(imageMin, avail);
        }
        else
        {
            mViewportHovered = false;
        }

        if(mViewportHovered && !ImGuizmo::IsUsing())
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

void EditorLayer::drawToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::SetCursorPos(ImVec2(8.0f, ImGui::GetCursorPosY() + 6.0f));

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

void EditorLayer::drawGizmos(const ImVec2 &imageMin, const ImVec2 &imageSize)
{
    if(imageSize.x < 1.0f || imageSize.y < 1.0f)
    {
        return;
    }

    const auto &projectionUbo = mRenderer->GetCurrentProjection();
    glm::mat4 view = projectionUbo.view;
    glm::mat4 proj = projectionUbo.projection;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);

    if(mDrawGrid)
    {
        const glm::mat4 identity(1.0f);
        ImGuizmo::DrawGrid(glm::value_ptr(view), glm::value_ptr(proj), glm::value_ptr(identity),
                           20.0f);
    }

    if(!mSelection->HasSelection())
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
