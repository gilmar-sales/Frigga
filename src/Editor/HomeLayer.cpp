#include "HomeLayer.hpp"

#include "BoostrapIconsFont.hpp"
#include "Paths/EditorPaths.hpp"
#include "UiScale.hpp"

#include <SDL3/SDL_dialog.h>

#include <cstdio>
#include <cstring>
#include <imgui.h>

namespace
{
    const SDL_DialogFileFilter kProjectFilters[] = {
        {"Frigga Project", "project"},
        {"All files", "*"},
    };
} // namespace

HomeLayer::HomeLayer(skr::Arc<ProjectSession> session, skr::Arc<fra::Window> window,
                     skr::Arc<EditorPreferences> preferences)
    : fg::Layer("Home Layer"), mSession(std::move(session)), mWindow(std::move(window)),
      mPreferences(std::move(preferences))
{
    EditorPaths::EnsureDirectories();
    const auto projects = EditorPaths::DefaultProjectsDir();
    std::snprintf(mParentDir, sizeof(mParentDir), "%s", projects.string().c_str());
}

void HomeLayer::onUpdate()
{
    processPendingDialogs();
}

void HomeLayer::onGui()
{
    if(mSession->IsInEditor())
    {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorUiScale::V(48.0f, 40.0f));
    if(ImGui::Begin("##FriggaHome", nullptr, flags))
    {
        ImGui::PopStyleVar();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.93f, 0.96f, 1.0f));
        ImGui::SetWindowFontScale(2.2f);
        ImGui::TextUnformatted("Frigga");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::TextDisabled("Create a gameplay project or open a recent one.");
        ImGui::Dummy(EditorUiScale::V(0.0f, 18.0f));

        const float columnWidth = ImGui::GetContentRegionAvail().x * 0.48f;
        ImGui::BeginChild("##NewProject", ImVec2(columnWidth, 0.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        drawNewProjectPanel();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, EditorUiScale::S(24.0f));

        ImGui::BeginChild("##RecentProjects", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        drawRecentProjects();
        ImGui::EndChild();

        if(!mUiError.empty())
        {
            ImGui::Dummy(EditorUiScale::V(0.0f, 12.0f));
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", mUiError.c_str());
        }
        if(!mSession->GetLastError().empty())
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s",
                               mSession->GetLastError().c_str());
        }

        ImGui::End();
    }
    else
    {
        ImGui::PopStyleVar();
    }
}

void HomeLayer::drawNewProjectPanel()
{
    ImGui::TextUnformatted("New project");
    ImGui::Separator();
    ImGui::Dummy(EditorUiScale::V(0.0f, 6.0f));

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##ProjectName", mProjectName, sizeof(mProjectName));
    ImGui::TextDisabled("Name");

    ImGui::Dummy(EditorUiScale::V(0.0f, 8.0f));
    ImGui::RadioButton("3D template", &mTemplateIndex, 0);
    ImGui::SameLine();
    ImGui::RadioButton("2D template", &mTemplateIndex, 1);
    ImGui::TextDisabled(mTemplateIndex == 1
                            ? "Top-down XZ plane + Player quad + Freyr system stub"
                            : "Cube + camera + light + Freyr system stub");

    ImGui::Dummy(EditorUiScale::V(0.0f, 8.0f));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - EditorUiScale::S(96.0f));
    ImGui::InputText("##ParentDir", mParentDir, sizeof(mParentDir));
    ImGui::SameLine();
    if(ImGui::Button("Browse…", EditorUiScale::V(88.0f, 0.0f)))
    {
        requestBrowseParentDialog();
    }
    ImGui::TextDisabled("Parent folder");

    ImGui::Dummy(EditorUiScale::V(0.0f, 14.0f));
    if(ImGui::Button(ICON_BTSP_FOLDERPLUS " Create project",
                     ImVec2(-1.0f, EditorUiScale::S(36.0f))))
    {
        mUiError.clear();
        const auto tmpl =
            mTemplateIndex == 1 ? fg::SceneTemplate::D2 : fg::SceneTemplate::D3;
        if(!mSession->CreateProject(mParentDir, mProjectName, tmpl))
        {
            mUiError = mSession->GetLastError();
        }
        else
        {
            mSession->OpenInCodeEditor();
        }
    }

    ImGui::Dummy(EditorUiScale::V(0.0f, 8.0f));
    if(ImGui::Button(ICON_BTSP_FOLDEROPEN " Open project…", ImVec2(-1.0f, 0.0f)))
    {
        requestOpenProjectDialog();
    }
}

void HomeLayer::drawRecentProjects()
{
    ImGui::TextUnformatted("Recent projects");
    ImGui::Separator();
    ImGui::Dummy(EditorUiScale::V(0.0f, 6.0f));

    if(mPreferences->recentProjects.empty())
    {
        ImGui::TextDisabled("No recent projects yet.");
        return;
    }

    for(const auto &entry : mPreferences->recentProjects)
    {
        ImGui::PushID(entry.path.c_str());
        const bool clicked = ImGui::Selectable(entry.name.c_str(), false,
                                               ImGuiSelectableFlags_AllowDoubleClick);
        ImGui::TextDisabled("%s", entry.path.c_str());
        if(!entry.openedAt.empty())
        {
            ImGui::TextDisabled("%s", entry.openedAt.c_str());
        }

        if(ImGui::Button(ICON_BTSP_CODE " Open in editor"))
        {
            mUiError.clear();
            if(!mSession->OpenInCodeEditor(entry.path))
            {
                mUiError = mSession->GetLastError();
            }
        }
        ImGui::SameLine();
        if(ImGui::Button(ICON_BTSP_FOLDEROPEN " Open project"))
        {
            mUiError.clear();
            if(!mSession->OpenProject(entry.path))
            {
                mUiError = mSession->GetLastError();
            }
        }

        if(clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            mUiError.clear();
            if(!mSession->OpenProject(entry.path))
            {
                mUiError = mSession->GetLastError();
            }
        }
        else if(clicked)
        {
            mUiError.clear();
            if(!mSession->OpenProject(entry.path))
            {
                mUiError = mSession->GetLastError();
            }
        }
        ImGui::Separator();
        ImGui::PopID();
    }
}

void HomeLayer::requestOpenProjectDialog()
{
    {
        std::lock_guard lock(mDialogMutex);
        mDialogDefaultLocation = mParentDir;
    }
    SDL_ShowOpenFileDialog(onOpenProjectDialog, this, mWindow->Get(), kProjectFilters,
                           static_cast<int>(std::size(kProjectFilters)),
                           mDialogDefaultLocation.c_str(), false);
}

void HomeLayer::requestBrowseParentDialog()
{
    {
        std::lock_guard lock(mDialogMutex);
        mDialogDefaultLocation = mParentDir;
    }
    SDL_ShowOpenFolderDialog(onBrowseParentDialog, this, mWindow->Get(),
                             mDialogDefaultLocation.c_str(), false);
}

void HomeLayer::onOpenProjectDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<HomeLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        return;
    }
    std::lock_guard lock(self->mDialogMutex);
    self->mPendingAction = PendingAction::OpenProject;
    self->mPendingPath   = filelist[0];
}

void HomeLayer::onBrowseParentDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<HomeLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        return;
    }
    std::lock_guard lock(self->mDialogMutex);
    self->mPendingAction = PendingAction::BrowseParent;
    self->mPendingPath   = filelist[0];
}

void HomeLayer::processPendingDialogs()
{
    PendingAction action = PendingAction::None;
    std::optional<std::filesystem::path> path;
    {
        std::lock_guard lock(mDialogMutex);
        action         = mPendingAction;
        path           = mPendingPath;
        mPendingAction = PendingAction::None;
        mPendingPath.reset();
    }

    if(action == PendingAction::None || !path)
    {
        return;
    }

    if(action == PendingAction::OpenProject)
    {
        mUiError.clear();
        if(!mSession->OpenProject(*path))
        {
            mUiError = mSession->GetLastError();
        }
    }
    else if(action == PendingAction::BrowseParent)
    {
        std::snprintf(mParentDir, sizeof(mParentDir), "%s", path->string().c_str());
    }
}
