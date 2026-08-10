#include "StatusBar.hpp"

#include "../BoostrapIconsFont.hpp"
#include "../UiScale.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <string>
#include <vector>

StatusBar::StatusBar(skr::Arc<ProjectSession> session, skr::Arc<fg::Scene> scene,
                     skr::Arc<fg::AssetRegistry> assets,
                     skr::Arc<fg::GameplayPluginHost> pluginHost,
                     skr::Arc<fg::SceneSimulationState> simulation)
    : mSession(std::move(session)), mScene(std::move(scene)), mAssets(std::move(assets)),
      mPluginHost(std::move(pluginHost)), mSimulation(std::move(simulation))
{
}

float StatusBar::Height()
{
    return EditorUiScale::S(24.0f);
}

void StatusBar::Draw(const ImGuiViewport *viewport)
{
    if(viewport == nullptr)
    {
        return;
    }

    const auto tasks = mSession->GetBackgroundTasks();
    if(mSession->HasRunningBackgroundTasks() && !mTasksExpanded)
    {
        // Keep collapsed by default; auto-open only on failure.
    }
    for(const auto &task : tasks)
    {
        if(task.state == EditorBackgroundTaskState::Failed)
        {
            mTasksExpanded = true;
            break;
        }
    }

    const float barHeight = Height();
    drawStrip(viewport, barHeight);
    if(mTasksExpanded)
    {
        drawTasksPanel(viewport, barHeight);
    }
}

void StatusBar::drawMiniProgress(float width)
{
    const auto tasks   = mSession->GetBackgroundTasks();
    const bool running = mSession->HasRunningBackgroundTasks();

    float progress     = 0.0f;
    bool determinate   = false;
    const char *label  = nullptr;
    ImVec4 tint        = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);

    if(!tasks.empty())
    {
        const auto &task = tasks.front();
        progress         = task.progress;
        determinate      = task.determinate;
        if(task.state == EditorBackgroundTaskState::Succeeded)
        {
            label = ICON_BTSP_CHECKCIRCLE;
            tint  = ImVec4(0.35f, 0.78f, 0.45f, 1.0f);
            progress = 1.0f;
        }
        else if(task.state == EditorBackgroundTaskState::Failed)
        {
            label = ICON_BTSP_CLOSECIRCLE;
            tint  = ImVec4(0.92f, 0.38f, 0.38f, 1.0f);
            progress = 1.0f;
        }
        else if(running)
        {
            label = ICON_BTSP_ACTIVITY;
        }
    }

    if(label == nullptr && !running && tasks.empty())
    {
        ImGui::TextDisabled("%s", ICON_BTSP_BELL);
        return;
    }

    if(label != nullptr)
    {
        ImGui::TextColored(tint, "%s", label);
        ImGui::SameLine();
    }

    const float barW = std::max(40.0f, width - EditorUiScale::S(28.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, tint);
    if(running && !determinate)
    {
        ImGui::ProgressBar(progress, ImVec2(barW, EditorUiScale::S(10.0f)), "");
    }
    else
    {
        char overlay[16] {};
        if(determinate && running)
        {
            std::snprintf(overlay, sizeof(overlay), "%.0f%%", progress * 100.0f);
        }
        ImGui::ProgressBar(progress, ImVec2(barW, EditorUiScale::S(10.0f)),
                           overlay[0] != '\0' ? overlay : "");
    }
    ImGui::PopStyleColor();
}

void StatusBar::drawStrip(const ImGuiViewport *viewport, float barHeight)
{
    const ImVec2 pos  = {viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - barHeight};
    const ImVec2 size = {viewport->WorkSize.x, barHeight};

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(EditorUiScale::S(8.0f), EditorUiScale::S(4.0f)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if(!ImGui::Begin("##FriggaStatusBar", nullptr, flags))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        return;
    }

    const char *playLabel = "Edit";
    ImVec4 playColor      = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    if(mSimulation->IsPlaying())
    {
        if(mSimulation->IsPaused())
        {
            playLabel = "Paused";
            playColor = ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
        }
        else
        {
            playLabel = "Play";
            playColor = ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
        }
    }

    const auto &desc       = mSession->GetDescriptor();
    const std::size_t models    = mAssets->GetModels().size();
    const std::size_t textures  = mAssets->GetTextures().size();
    const std::size_t materials = mAssets->GetMaterials().size();
    const bool pluginLoaded     = mPluginHost->IsLoaded();
    const std::size_t typeCount =
        pluginLoaded ? mPluginHost->GetRegisteredTypeIds().size() : 0;

    const std::string stats = std::format(
        "{}  ·  {}  ·  {} models  ·  {} textures  ·  {} mats  ·  plugin {}  ·  ",
        desc.name.empty() ? "Project" : desc.name, mScene->GetDisplayName(), models, textures,
        materials, pluginLoaded ? std::format("{} types", typeCount) : "unloaded");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(stats.c_str());
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextColored(playColor, "%s", playLabel);

    const auto status = mSession->GetStatusMessage();
    if(!status.empty())
    {
        ImGui::SameLine(0.0f, EditorUiScale::S(12.0f));
        ImGui::TextDisabled("%s", status.c_str());
    }

    const float progressWidth = EditorUiScale::S(140.0f);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - progressWidth);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + EditorUiScale::S(2.0f));

    const ImVec2 progressMin = ImGui::GetCursorScreenPos();
    drawMiniProgress(progressWidth);
    const ImVec2 progressMax = {progressMin.x + progressWidth,
                                progressMin.y + ImGui::GetTextLineHeightWithSpacing()};

    if(ImGui::IsMouseHoveringRect(progressMin, progressMax) &&
       ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        mTasksExpanded = !mTasksExpanded;
    }
    if(ImGui::IsMouseHoveringRect(progressMin, progressMax))
    {
        ImGui::SetTooltip("%s", mTasksExpanded ? "Hide background tasks" : "Show background tasks");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void StatusBar::drawTasksPanel(const ImGuiViewport *viewport, float barHeight)
{
    const float panelHeight = EditorUiScale::S(220.0f);
    const float panelWidth  = std::min(viewport->WorkSize.x * 0.42f, EditorUiScale::S(480.0f));
    const ImVec2 size       = {panelWidth, panelHeight};
    const ImVec2 pos        = {viewport->WorkPos.x + viewport->WorkSize.x - panelWidth -
                                   EditorUiScale::S(8.0f),
                         viewport->WorkPos.y + viewport->WorkSize.y - barHeight - panelHeight -
                             EditorUiScale::S(4.0f)};

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, EditorUiScale::S(4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorUiScale::V(10.0f, 8.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar;

    if(!ImGui::Begin("##FriggaBackgroundTasks", nullptr, flags))
    {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    ImGui::TextUnformatted("Background Tasks");
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - EditorUiScale::S(18.0f));
    if(ImGui::SmallButton(ICON_BTSP_CLOSECIRCLE "##closeTasks"))
    {
        mTasksExpanded = false;
    }

    ImGui::Separator();

    const auto tasks = mSession->GetBackgroundTasks();
    if(tasks.empty())
    {
        ImGui::TextDisabled("No background tasks.");
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    for(const auto &task : tasks)
    {
        ImGui::PushID(task.id.c_str());

        ImVec4 stateColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        const char *icon  = ICON_BTSP_ACTIVITY;
        if(task.state == EditorBackgroundTaskState::Succeeded)
        {
            stateColor = ImVec4(0.35f, 0.78f, 0.45f, 1.0f);
            icon       = ICON_BTSP_CHECKCIRCLE;
        }
        else if(task.state == EditorBackgroundTaskState::Failed)
        {
            stateColor = ImVec4(0.92f, 0.38f, 0.38f, 1.0f);
            icon       = ICON_BTSP_CLOSECIRCLE;
        }

        ImGui::TextColored(stateColor, "%s  %s", icon, task.title.c_str());
        ImGui::TextDisabled("%s", task.detail.c_str());

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, stateColor);
        if(task.state == EditorBackgroundTaskState::Running && !task.determinate)
        {
            ImGui::ProgressBar(task.progress, ImVec2(-1.0f, EditorUiScale::S(12.0f)), "");
        }
        else
        {
            char overlay[16] {};
            if(task.state == EditorBackgroundTaskState::Running && task.determinate)
            {
                std::snprintf(overlay, sizeof(overlay), "%.0f%%", task.progress * 100.0f);
            }
            else if(task.state == EditorBackgroundTaskState::Succeeded)
            {
                std::snprintf(overlay, sizeof(overlay), "Done");
            }
            else if(task.state == EditorBackgroundTaskState::Failed)
            {
                std::snprintf(overlay, sizeof(overlay), "Failed");
            }
            ImGui::ProgressBar(task.progress, ImVec2(-1.0f, EditorUiScale::S(12.0f)), overlay);
        }
        ImGui::PopStyleColor();

        if(!task.logTail.empty())
        {
            ImGui::BeginChild("##taskLog", ImVec2(0.0f, EditorUiScale::S(100.0f)),
                              ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(task.logTail.c_str());
            if(task.state == EditorBackgroundTaskState::Running)
            {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();

            if(ImGui::Button("Copy log"))
            {
                ImGui::SetClipboardText(task.logTail.c_str());
            }
            ImGui::SameLine();
        }

        const bool running = task.state == EditorBackgroundTaskState::Running;
        ImGui::BeginDisabled(running);
        if(ImGui::Button("Dismiss"))
        {
            mSession->DismissBuildUi();
            if(!mSession->HasRunningBackgroundTasks() &&
               mSession->GetBackgroundTasks().empty())
            {
                mTasksExpanded = false;
            }
        }
        ImGui::EndDisabled();

        ImGui::PopID();
        ImGui::Spacing();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}
