#include "MainLayer.hpp"

#include "BoostrapIconsFont.hpp"
#include "DockLayout.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "SelectionContext.hpp"
#include "Workflows/AnimationWorkflow.hpp"
#include "Workflows/AudioWorkflow.hpp"
#include "Workflows/EcsWorkflow.hpp"
#include "Workflows/GamePlayWorkflow.hpp"
#include "Workflows/ScriptingWorkflow.hpp"
#include "Workflows/ShadingWorkflow.hpp"

#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Gui/Extensions/Extensions.hpp>

#include <SDL3/SDL_dialog.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <imgui.h>
#include <imgui_internal.h>

namespace
{
    const SDL_DialogFileFilter kSceneFilters[] = {
        {"Frigga Scene", "json"},
        {"All files", "*"},
    };

    std::filesystem::path EnsureSceneExtension(std::filesystem::path path)
    {
        if(!path.has_extension())
        {
            path += ".json";
        }
        return path;
    }
} // namespace

MainLayer::MainLayer(skr::Arc<fg::Scene> scene, skr::Arc<fg::LayerStack> layerStack,
                     skr::Arc<fra::Window> window, skr::Arc<skr::ServiceProvider> serviceProvider,
                     skr::Arc<ProjectSession> session)
    : fg::Layer("Dock Layer"), mScene(std::move(scene)), mLayerStack(std::move(layerStack)),
      mWindow(std::move(window)), mHierarchy(serviceProvider->GetService<HierarchyLayer>()),
      mSelection(serviceProvider->GetService<SelectionContext>()),
      mSimulation(serviceProvider->GetService<fg::SceneSimulationState>()),
      mSession(std::move(session))
{
    m_tabIds = {
        {"Gameplay",  serviceProvider->GetService<GamePlayWorkflow>() },
        {"Animation", serviceProvider->GetService<AnimationWorkflow>()},
        {"Audio",     serviceProvider->GetService<AudioWorkflow>()    },
        {"Shading",   serviceProvider->GetService<ShadingWorkflow>()  },
        {"Scripting", serviceProvider->GetService<ScriptingWorkflow>()},
        {"ECS",       serviceProvider->GetService<EcsWorkflow>()      }
    };

    m_activeTab     = m_tabIds.begin()->second;
    m_activeTabName = m_tabIds.begin()->first;
}

void MainLayer::onUpdate()
{
    mSession->Poll();

    if(!mSession->IsInEditor())
    {
        return;
    }

    processPendingSceneActions();
    handleShortcuts();

    if(m_activeTab)
    {
        m_activeTab->onUpdate();
    }
}

void MainLayer::handleShortcuts()
{
    const ImGuiIO &io = ImGui::GetIO();
    if(io.WantTextInput)
    {
        return;
    }

    if(io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_E, false))
    {
        if(mSession->HasProject())
        {
            mSession->OpenInCodeEditor();
        }
        return;
    }

    if(io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        if(!mSession->IsBuilding())
        {
            mSession->ReloadPlugin();
        }
        return;
    }

    if(io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P, false))
    {
        mSimulation->Stop();
        return;
    }

    if(io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        mSimulation->ToggleShowColliders();
        return;
    }

    if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
    {
        mSimulation->TogglePlayPause();
        return;
    }

    if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Period, false))
    {
        mSimulation->Step();
        return;
    }

    if(!io.KeyCtrl)
    {
        return;
    }

    if(mSimulation->IsPlaying())
    {
        return;
    }

    if(io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        requestSaveSceneAs();
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        requestSaveScene();
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_N, false))
    {
        requestNewScene();
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_O, false))
    {
        requestOpenScene();
    }
}

void MainLayer::ensureEditMode()
{
    if(mSimulation->IsPlaying())
    {
        mSimulation->Stop();
    }
}

void MainLayer::processPendingSceneActions()
{
    PendingSceneAction action = PendingSceneAction::None;
    std::optional<std::filesystem::path> path;
    {
        std::lock_guard lock(mDialogMutex);
        action        = mPendingAction;
        path          = mPendingPath;
        mPendingAction = PendingSceneAction::None;
        mPendingPath.reset();
    }

    if(action == PendingSceneAction::None)
    {
        return;
    }

    mSelection->Clear();
    ensureEditMode();

    switch(action)
    {
    case PendingSceneAction::New:
        mScene->NewScene();
        break;
    case PendingSceneAction::Open:
        if(path)
        {
            mScene->LoadScene(*path);
        }
        break;
    case PendingSceneAction::SaveAs:
        if(path)
        {
            mScene->SaveScene(EnsureSceneExtension(*path));
        }
        break;
    case PendingSceneAction::None:
        break;
    }
}

void MainLayer::requestNewScene()
{
    std::lock_guard lock(mDialogMutex);
    mPendingAction = PendingSceneAction::New;
    mPendingPath.reset();
}

void MainLayer::requestOpenScene()
{
    openSceneDialog();
}

void MainLayer::requestSaveScene()
{
    if(mScene->HasPath())
    {
        mScene->SaveScene();
        return;
    }
    requestSaveSceneAs();
}

void MainLayer::requestSaveSceneAs()
{
    saveSceneDialog();
}

void MainLayer::onOpenSceneDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<MainLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        return;
    }

    std::lock_guard lock(self->mDialogMutex);
    self->mPendingAction = PendingSceneAction::Open;
    self->mPendingPath   = filelist[0];
}

void MainLayer::onSaveSceneDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<MainLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        return;
    }

    std::lock_guard lock(self->mDialogMutex);
    self->mPendingAction = PendingSceneAction::SaveAs;
    self->mPendingPath   = filelist[0];
}

void MainLayer::openSceneDialog()
{
    {
        std::lock_guard lock(mDialogMutex);
        mDialogDefaultLocation =
            mScene->HasPath() ? mScene->GetPath().string() : std::string {};
    }

    const char *defaultLocation =
        mDialogDefaultLocation.empty() ? nullptr : mDialogDefaultLocation.c_str();
    SDL_ShowOpenFileDialog(onOpenSceneDialog, this, mWindow->Get(), kSceneFilters,
                           static_cast<int>(std::size(kSceneFilters)), defaultLocation, false);
}

void MainLayer::saveSceneDialog()
{
    {
        std::lock_guard lock(mDialogMutex);
        mDialogDefaultLocation =
            mScene->HasPath() ? mScene->GetPath().string() : std::string {"untitled.json"};
    }

    SDL_ShowSaveFileDialog(onSaveSceneDialog, this, mWindow->Get(), kSceneFilters,
                           static_cast<int>(std::size(kSceneFilters)),
                           mDialogDefaultLocation.c_str());
}

void MainLayer::onGui()
{
    if(!mSession->IsInEditor())
    {
        return;
    }

    auto ctx = ImGui::GetCurrentContext();
    static ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    static ImGuiIO &io = ImGui::GetIO();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    if(ImGui::Begin("DockSpace", nullptr, window_flags))
    {
        auto mainWindow = ImGui::GetCurrentWindow();
        ImGui::PopStyleVar();

        if(io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            EditorDock::SetLayoutId(m_activeTabName);

            auto dockspace_flags = ImGuiDockNodeFlags_None;
            // One dockspace tree per workflow tab so layouts stay independent.
            auto dockspace_id    = ImGui::GetID(m_activeTabName);

            if(m_resetDockLayout || ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
            {
                m_resetDockLayout = false;
                m_activeTab->buildDefaultDockLayout(dockspace_id);
            }

            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        m_activeTab->onGui();

        drawTitleBar();

        drawMenuBar();

        drawBuildProgressOverlay();

        ImGui::End();
    }
}

void MainLayer::drawBuildProgressOverlay()
{
    if(mSession->IsBuilding() || mSession->GetBuildPhase() == PluginBuildPhase::Reloading)
    {
        mShowBuildOverlay = true;
    }

    if(!mShowBuildOverlay)
    {
        return;
    }

    const auto phase    = mSession->GetBuildPhase();
    const bool building =
        mSession->IsBuilding() || phase == PluginBuildPhase::Reloading;

    ImGui::OpenPopup("##PluginBuildProgress");
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Always);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;

    if(!ImGui::BeginPopupModal("##PluginBuildProgress", nullptr, flags))
    {
        return;
    }

    const char *title = "Building gameplay plugin";
    switch(phase)
    {
    case PluginBuildPhase::Configuring:
        title = "Configuring plugin (CMake)…";
        break;
    case PluginBuildPhase::Building:
        title = "Compiling gameplay plugin…";
        break;
    case PluginBuildPhase::Reloading:
        title = "Reloading plugin…";
        break;
    case PluginBuildPhase::Succeeded:
        title = "Plugin build succeeded";
        break;
    case PluginBuildPhase::Failed:
        title = "Plugin build failed";
        break;
    default:
        break;
    }

    ImGui::TextUnformatted(title);
    ImGui::Spacing();

    const float progress = mSession->GetBuildProgress();
    char        overlay[32];
    if(mSession->IsBuildProgressDeterminate() && mSession->IsBuilding())
    {
        std::snprintf(overlay, sizeof(overlay), "%.0f%%", progress * 100.0f);
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), overlay);
    }
    else if(building)
    {
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), "");
    }
    else if(phase == PluginBuildPhase::Succeeded)
    {
        ImGui::ProgressBar(1.0f, ImVec2(-1.0f, 0.0f), "Done");
    }
    else if(phase == PluginBuildPhase::Failed)
    {
        ImGui::ProgressBar(1.0f, ImVec2(-1.0f, 0.0f), "Failed");
    }

    const auto logTail = mSession->GetBuildLogTail();
    if(!logTail.empty())
    {
        ImGui::Spacing();
        ImGui::BeginChild("##BuildLog", ImVec2(0.0f, 160.0f), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(logTail.c_str());
        if(building)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    if(phase == PluginBuildPhase::Failed)
    {
        const auto err = mSession->GetLastError();
        if(!err.empty())
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", err.c_str());
        }
    }

    ImGui::Spacing();
    if(!logTail.empty())
    {
        if(ImGui::Button("Copy log", ImVec2(120.0f, 0.0f)))
        {
            ImGui::SetClipboardText(logTail.c_str());
        }
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(building);
    if(ImGui::Button("Close", ImVec2(120.0f, 0.0f)) ||
       (!building && ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
    {
        mShowBuildOverlay = false;
        mSession->DismissBuildUi();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();

    ImGui::EndPopup();
}

float MainLayer::drawTitleBar()
{
    const float titlebar_height = 48.0f;
    const ImVec2 windowPadding  = ImGui::GetCurrentWindow()->WindowPadding;
    (void)windowPadding;
    return titlebar_height;
}

void MainLayer::drawMenuBar()
{
    const float titlebar_height = 26.0f;

    // glm::vec2 pos = fg::Application::GetWindow()->getMousePos();
    //
    // static glm::vec2 lastPos = pos;
    // if(ImGui::IsMouseDown(0) && pos.y <= titlebar_height)
    // {
    //     static double moveOffsetX = 0;
    //     static double moveOffsetY = 0;
    //
    //     moveOffsetX = pos.x - lastPos.x;
    //     moveOffsetY = pos.y - lastPos.y;
    //
    //     int xpos, ypos;
    //     GLFWwindow *window = (GLFWwindow *)fg::Application::GetWindow()->getNativeWindow();
    //     glfwGetWindowPos(window, &xpos, &ypos);
    //
    //     glfwSetWindowPos(window, floor(xpos) + moveOffsetX, floor(ypos) + moveOffsetY);
    // }
    //
    // lastPos = pos;

    if(ImGui::BeginMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem(ICON_BTSP_FOLDER " Back to Home"))
            {
                if(!mSession->IsBuilding())
                {
                    mSession->CloseToHome();
                }
            }
            ImGui::Separator();
            ImGui::BeginDisabled(mSimulation->IsPlaying());
            if(ImGui::MenuItem(ICON_BTSP_FOLDERPLUS " New Scene", "Ctrl+N"))
            {
                requestNewScene();
            }
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_BTSP_FOLDEROPEN " Open Scene...", "Ctrl+O"))
            {
                requestOpenScene();
            }
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_BTSP_FOLDERSYMLINK " Save", "Ctrl+S"))
            {
                requestSaveScene();
            }
            if(ImGui::MenuItem(ICON_BTSP_FOLDERSYMLINK " Save As...", "Ctrl+Shift+S"))
            {
                requestSaveSceneAs();
            }
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::BeginDisabled(!mSession->HasProject() || mSimulation->IsPlaying() ||
                                 mSession->IsBuilding());
            if(ImGui::MenuItem(ICON_BTSP_CODE " Open in Code Editor", "Ctrl+Shift+E"))
            {
                mSession->OpenInCodeEditor();
            }
            if(ImGui::MenuItem("Build Gameplay Plugin"))
            {
                if(mSession->BuildPlugin())
                {
                    mShowBuildOverlay = true;
                }
            }
            if(ImGui::MenuItem("Migrate Project Files"))
            {
                mSession->MigrateOpenProject(true);
            }
            if(ImGui::MenuItem("Reload Gameplay Plugin", "Ctrl+R"))
            {
                mSession->ReloadPlugin();
            }
            ImGui::EndDisabled();
            {
                const auto status = mSession->GetStatusMessage();
                if(!status.empty())
                {
                    ImGui::TextDisabled("%s", status.c_str());
                }
            }
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_BTSP_SHUTDOWN " Close Phantom", "Alt+F4"))
            {
                mWindow->Close();
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Edit"))
        {
            if(ImGui::MenuItem("Undo...", "Ctrl+N"))
            { /* Do stuff */
            }
            if(ImGui::MenuItem("Redo...", "Ctrl+O"))
            { /* Do stuff */
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Cut", "Ctrl+X"))
            { /* Do stuff */
            }
            if(ImGui::MenuItem("Copy", "Ctrl+C"))
            { /* Do stuff */
            }
            if(ImGui::MenuItem("Paste", "Ctrl+V"))
            { /* Do stuff */
            }
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_BTSP_GEAR " Preferences...", "Ctrl+,"))
            {
                PreferencesLayer::IsOpen = true;
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Entity"))
        {
            ImGui::BeginDisabled(mSimulation->IsPlaying());
            if(ImGui::MenuItem("Create Empty"))
            {
                mHierarchy->createEmptyEntity();
            }
            if(ImGui::BeginMenu("Create Geometry"))
            {
                using fg::PrimitiveType;
                for(auto type: {PrimitiveType::Cube, PrimitiveType::Sphere, PrimitiveType::Capsule,
                                PrimitiveType::Cylinder, PrimitiveType::Cone, PrimitiveType::Plane,
                                PrimitiveType::Quad})
                {
                    if(ImGui::MenuItem(fg::PrimitiveMeshFactory::GetDisplayName(type)))
                    {
                        mHierarchy->createPrimitiveEntity(type);
                    }
                }
                ImGui::EndMenu();
            }
            if(ImGui::MenuItem("Create Camera"))
            {
                mHierarchy->createCameraEntity();
            }
            if(ImGui::BeginMenu(ICON_BTSP_LIGHT " Create Light"))
            {
                for(auto type:
                    {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot,
                     fra::LightType::Area})
                {
                    const auto label = std::format("{} {}", HierarchyLayer::getLightIcon(type),
                                                   HierarchyLayer::getLightDisplayName(type));
                    if(ImGui::MenuItem(label.c_str()))
                    {
                        mHierarchy->createLightEntity(type);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Component"))
        {
            if(ImGui::BeginMenu("Add Component"))
            {
                ImGui::BeginDisabled(!mSelection->HasSelection() || mSimulation->IsPlaying());
                if(ImGui::MenuItem("Rigid Body"))
                {
                    mHierarchy->addRigidBodyToSelection();
                }
                if(ImGui::BeginMenu(ICON_BTSP_LIGHT " Light"))
                {
                    for(auto type:
                        {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot,
                         fra::LightType::Area})
                    {
                        const auto label = std::format("{} {}", HierarchyLayer::getLightIcon(type),
                                                       HierarchyLayer::getLightDisplayName(type));
                        if(ImGui::MenuItem(label.c_str()))
                        {
                            mHierarchy->addLightToSelection(type);
                        }
                    }
                    ImGui::EndMenu();
                }
                if(mSelection->HasSelection())
                {
                    mHierarchy->drawGameplayAddComponentMenu(mSelection->Get());
                }
                ImGui::EndDisabled();
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Game"))
        {
            if(!mSimulation->IsPlaying())
            {
                if(ImGui::MenuItem(ICON_BTSP_PLAY " Play", "Ctrl+P"))
                {
                    mSimulation->Play();
                }
            }
            else if(mSimulation->IsPaused())
            {
                if(ImGui::MenuItem(ICON_BTSP_PLAY " Resume", "Ctrl+P"))
                {
                    mSimulation->Resume();
                }
            }
            else
            {
                if(ImGui::MenuItem(ICON_BTSP_PAUSE " Pause", "Ctrl+P"))
                {
                    mSimulation->Pause();
                }
            }

            ImGui::BeginDisabled(!mSimulation->IsPlaying());
            if(ImGui::MenuItem(ICON_BTSP_SKIPFORWARD " Step", "Ctrl+."))
            {
                mSimulation->Step();
            }
            if(ImGui::MenuItem(ICON_BTSP_SKIPEND " Stop", "Ctrl+Shift+P"))
            {
                mSimulation->Stop();
            }
            ImGui::EndDisabled();

            ImGui::Separator();
            bool showColliders = mSimulation->GetShowColliders();
            if(ImGui::MenuItem(ICON_BTSP_BOUNDINGBOX " Show Colliders", "Ctrl+Shift+C",
                               showColliders))
            {
                mSimulation->SetShowColliders(!showColliders);
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Window"))
        {
            if(ImGui::MenuItem("Reset Layout"))
            {
                m_resetDockLayout = true;
            }
            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x * 0.5f - 150);

        ImGui::Text("%s", mScene->GetDisplayName().c_str());
        if(mSimulation->IsPlaying())
        {
            ImGui::SameLine();
            if(mSimulation->IsPaused())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), ICON_BTSP_PAUSE " PAUSED");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.0f), ICON_BTSP_PLAY " PLAY");
            }
        }

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 350);

        ImGui::BeginTabBar("##GamePlayTabs");

        for(auto &tabPair: m_tabIds)
        {
            if(ImGui::BeginTabItem(tabPair.first))
            {
                m_activeTab     = tabPair.second;
                m_activeTabName = tabPair.first;
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();

        ImGui::EndMenuBar();
    }
}