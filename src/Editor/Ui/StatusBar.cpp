#include "StatusBar.hpp"
#include <Frigga/Asset/AssetRegistry.hpp>

#include "../BoostrapIconsFont.hpp"
#include "../UiScale.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <string>
#include <vector>

StatusBar::StatusBar(skr::Arc<ProjectSession> session, skr::Arc<fg::Scene> scene,
                     skr::Arc<fg::AssetRegistry> assets,
                     skr::Arc<fg::GameplayModuleHost> moduleHost,
                     skr::Arc<fg::SceneSimulationState> simulation)
    : mSession(std::move(session)), mScene(std::move(scene)), mAssets(std::move(assets)),
      mModuleHost(std::move(moduleHost)), mSimulation(std::move(simulation))
{
}

float StatusBar::Height()
{
    // Empty window (no context yet) falls back to the legacy fixed size.
    if(ImGui::GetCurrentContext() == nullptr)
    {
        return EditorUiScale::S(24.0f);
    }

    // Strip height must fit the tallest control (small buttons / text / progress bar)
    // plus the window's vertical padding, otherwise the content clips mid-height.
    const float vPadding = EditorUiScale::S(4.0f) * 2.0f;
    return ImGui::GetFrameHeight() + vPadding;
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
    for(const auto &task: tasks)
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

    float progress    = 0.0f;
    bool determinate  = false;
    const char *label = nullptr;
    ImVec4 tint       = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);

    if(!tasks.empty())
    {
        const auto &task = tasks.front();
        progress         = task.progress;
        determinate      = task.determinate;
        if(task.state == EditorBackgroundTaskState::Succeeded)
        {
            label    = ICON_BTSP_CHECKCIRCLE;
            tint     = ImVec4(0.35f, 0.78f, 0.45f, 1.0f);
            progress = 1.0f;
        }
        else if(task.state == EditorBackgroundTaskState::Failed)
        {
            label    = ICON_BTSP_CLOSECIRCLE;
            tint     = ImVec4(0.92f, 0.38f, 0.38f, 1.0f);
            progress = 1.0f;
        }
        else if(running)
        {
            label = ICON_BTSP_ACTIVITY;
        }
    }

    if(label == nullptr && !running && tasks.empty())
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", ICON_BTSP_BELL);
        return;
    }

    const float rowH  = ImGui::GetFrameHeight();
    const float textH = ImGui::GetTextLineHeight();
    const float barH  = std::max(rowH - EditorUiScale::S(4.0f), textH + EditorUiScale::S(2.0f));
    const float rowY  = ImGui::GetCursorPosY();

    float iconW = 0.0f;
    if(label != nullptr)
    {
        ImGui::SetCursorPosY(rowY + (rowH - textH) * 0.5f);
        ImGui::TextColored(tint, "%s", label);
        iconW = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLine(0.0f, 0.0f);
    }

    ImGui::SetCursorPosY(rowY + (rowH - barH) * 0.5f);
    const float barW = std::max(EditorUiScale::S(40.0f), width - iconW);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, tint);
    if(running && !determinate)
    {
        ImGui::ProgressBar(progress, ImVec2(barW, barH), "");
    }
    else
    {
        char overlay[16]{};
        if(determinate && running)
        {
            std::snprintf(overlay, sizeof(overlay), "%.0f%%", progress * 100.0f);
        }
        ImGui::ProgressBar(progress, ImVec2(barW, barH), overlay[0] != '\0' ? overlay : "");
    }
    ImGui::PopStyleColor();
}

void StatusBar::drawStrip(const ImGuiViewport *viewport, float barHeight)
{
    const ImVec2 pos  = {viewport->WorkPos.x,
                         viewport->WorkPos.y + viewport->WorkSize.y - barHeight};
    const ImVec2 size = {viewport->WorkSize.x, barHeight};

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(EditorUiScale::S(8.0f), EditorUiScale::S(4.0f)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
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

    const auto &desc            = mSession->GetDescriptor();
    const std::size_t models    = mAssets->GetModels().size();
    const std::size_t textures  = mAssets->GetTextures().size();
    const std::size_t materials = mAssets->GetMaterials().size();
    const bool moduleLoaded     = mModuleHost->IsLoaded();
    const std::size_t typeCount = moduleLoaded ? mModuleHost->GetRegisteredTypeIds().size() : 0;

    const std::string stats = std::format(
        "{}  ·  {}  ·  {} models  ·  {} textures  ·  {} mats  ·  module {}  ·  ",
        desc.name.empty() ? "Project" : desc.name, mScene->GetDisplayName(), models, textures,
        materials,
        moduleLoaded ? std::format("{} modules, {} types", mModuleHost->LoadedCount(), typeCount)
                     : "unloaded");

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

    // Right cluster: module actions + mini task progress. The strip content height
    // is exactly one frame height; center each control in it explicitly instead of
    // nudging by a constant so the row stays centered at any DPI/scale.
    const float contentH = ImGui::GetFrameHeight();
    // SmallButton height given the FramePadding pushed below (y = S(1) per side).
    const float actionH = ImGui::GetTextLineHeight() + EditorUiScale::S(2.0f);

    const float progressWidth    = EditorUiScale::S(140.0f);
    const float moduleGroupWidth = EditorUiScale::S(8.0f) + EditorUiScale::S(72.0f) +
                                   EditorUiScale::S(6.0f) + EditorUiScale::S(74.0f) +
                                   EditorUiScale::S(12.0f);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - progressWidth - moduleGroupWidth);
    const float clusterTop = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(clusterTop + std::max(0.0f, (contentH - actionH) * 0.5f));

    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            ImVec2(EditorUiScale::S(6.0f), EditorUiScale::S(1.0f)));
        const bool busy    = mSession->IsBuilding();
        const bool playing = mSimulation->IsPlaying();
        ImGui::BeginDisabled(busy || playing || !mSession->HasProject());
        if(ImGui::SmallButton(ICON_BTSP_HAMMER " Build##statusBuild"))
        {
            mSession->BuildModule();
            mTasksExpanded = true;
        }
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Build modules (Ctrl+B)");
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, EditorUiScale::S(6.0f));
        ImGui::BeginDisabled(busy || !mSession->HasProject());
        if(ImGui::SmallButton(ICON_BTSP_RELOAD " Reload##statusReload"))
        {
            mSession->ReloadModule();
        }
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Reload modules (Ctrl+R)");
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
    }

    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - progressWidth);
    // The mini progress row is a full frame tall (see drawMiniProgress), so pin it
    // to the content top; it then fills the strip exactly and stays centered.
    ImGui::SetCursorPosY(clusterTop + std::max(0.0f, (contentH - ImGui::GetFrameHeight()) * 0.5f));

    const ImVec2 progressMin = ImGui::GetCursorScreenPos();
    drawMiniProgress(progressWidth);
    const ImVec2 progressMax = {progressMin.x + progressWidth,
                                progressMin.y + ImGui::GetFrameHeight()};

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
    const auto tasks = mSession->GetBackgroundTasks();
    if(tasks.empty())
    {
        mTaskIndex = 0;
    }
    else if(mTaskIndex >= tasks.size())
    {
        mTaskIndex = tasks.size() - 1;
    }

    // Fixed width, auto height (clamped): the popup grows upward from the strip
    // and only the log viewer scrolls, so one notification fits without clipping.
    const float panelWidth = std::min(viewport->WorkSize.x * 0.42f, EditorUiScale::S(440.0f));
    const float minH       = EditorUiScale::S(96.0f);
    const float maxH       = std::min(viewport->WorkSize.y * 0.6f, EditorUiScale::S(360.0f));

    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - EditorUiScale::S(8.0f),
               viewport->WorkPos.y + viewport->WorkSize.y - barHeight - EditorUiScale::S(4.0f)),
        ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(panelWidth, minH), ImVec2(panelWidth, maxH));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, EditorUiScale::S(4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorUiScale::V(10.0f, 8.0f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    if(!ImGui::Begin("##FriggaBackgroundTasks", nullptr, flags))
    {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Background Tasks");
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - EditorUiScale::S(18.0f));
    if(ImGui::SmallButton(ICON_BTSP_CLOSECIRCLE "##closeTasks"))
    {
        mTasksExpanded = false;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Hide background tasks");
    }

    ImGui::Separator();

    if(tasks.empty())
    {
        ImGui::TextDisabled("No background tasks.");
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    // Carousel controls (only when there is more than one notification).
    if(tasks.size() > 1)
    {
        const float navBtn = EditorUiScale::S(24.0f);
        const float navGap = EditorUiScale::S(6.0f);
        char counter[32]{};
        std::snprintf(counter, sizeof(counter), "%zu of %zu", mTaskIndex + 1, tasks.size());
        const float counterW = ImGui::CalcTextSize(counter).x;
        const float groupW   = navBtn * 2.0f + counterW + navGap * 2.0f;
        const float availW   = ImGui::GetContentRegionAvail().x;
        if(availW > groupW)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - groupW) * 0.5f);
        }

        ImGui::BeginDisabled(mTaskIndex == 0);
        if(ImGui::Button(ICON_BTSP_CHEVRONLEFT "##taskPrev", ImVec2(navBtn, 0.0f)))
        {
            --mTaskIndex;
        }
        ImGui::EndDisabled();
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Previous task");
        }
        ImGui::SameLine(0.0f, navGap);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", counter);
        ImGui::SameLine(0.0f, navGap);
        ImGui::BeginDisabled(mTaskIndex + 1 >= tasks.size());
        if(ImGui::Button(ICON_BTSP_CHEVRONRIGHT "##taskNext", ImVec2(navBtn, 0.0f)))
        {
            ++mTaskIndex;
        }
        ImGui::EndDisabled();
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Next task");
        }

        ImGui::Separator();
    }

    drawTaskCard(tasks[mTaskIndex]);

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void StatusBar::drawTaskCard(const EditorBackgroundTask &task)
{
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
    if(!task.detail.empty())
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", task.detail.c_str());
        ImGui::PopTextWrapPos();
    }

    // Auto-height bar so the overlay text ("75%", "Done", "Failed") always fits.
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, stateColor);
    if(task.state == EditorBackgroundTaskState::Running && !task.determinate)
    {
        ImGui::ProgressBar(task.progress, ImVec2(-1.0f, 0.0f), "");
    }
    else
    {
        char overlay[16]{};
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
        ImGui::ProgressBar(task.progress, ImVec2(-1.0f, 0.0f), overlay);
    }
    ImGui::PopStyleColor();

    if(!task.logTail.empty())
    {
        ImGui::BeginChild("##taskLog", ImVec2(0.0f, EditorUiScale::S(72.0f)),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(task.logTail.c_str());
        if(task.state == EditorBackgroundTaskState::Running)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    const bool running = task.state == EditorBackgroundTaskState::Running;
    ImGui::BeginDisabled(running);
    if(ImGui::Button("Dismiss"))
    {
        mSession->DismissBuildUi();
        if(!mSession->HasRunningBackgroundTasks() && mSession->GetBackgroundTasks().empty())
        {
            mTasksExpanded = false;
        }
    }
    ImGui::EndDisabled();
    if(!task.logTail.empty())
    {
        ImGui::SameLine();
        if(ImGui::Button("Copy log"))
        {
            ImGui::SetClipboardText(task.logTail.c_str());
        }
    }
}
