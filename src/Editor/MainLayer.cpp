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
                     skr::Arc<fra::Window> window, skr::Arc<skr::ServiceProvider> serviceProvider)
    : fg::Layer("Dock Layer"), mScene(std::move(scene)), mLayerStack(std::move(layerStack)),
      mWindow(std::move(window)), mHierarchy(serviceProvider->GetService<HierarchyLayer>()),
      mSelection(serviceProvider->GetService<SelectionContext>()),
      mSimulation(serviceProvider->GetService<fg::SceneSimulationState>())
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

        ImGui::End();
    }
}

float MainLayer::drawTitleBar()
{
    const float titlebar_height = 48.0f;
    const ImVec2 windowPadding  = ImGui::GetCurrentWindow()->WindowPadding;

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