#include "MainLayer.hpp"

#include "BoostrapIconsFont.hpp"
#include "Panels/PreferencesLayer.hpp"

#include <cmath>
#include <imgui.h>

#include <Frigga/Gui/Extensions/Extensions.hpp>

MainLayer::MainLayer(Ref<fg::Scene> scene, Ref<fg::LayerStack> layerStack, Ref<fra::Window> window)
    : fg::Layer("Dock Layer"), mScene(scene), mLayerStack(layerStack), mWindow(window)
{
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
            auto dockspace_flags = ImGuiDockNodeFlags_None;
            auto dockspace_id    = ImGui::GetID("MainDockSpace");

            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

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
            if(ImGui::MenuItem(ICON_BTSP_FOLDERPLUS " New Project...", "Ctrl+N"))
            { /* Do stuff */
            }
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_BTSP_FOLDEROPEN " Open Project...", "Ctrl+O"))
            { /* Do stuff */
            }
            if(ImGui::MenuItem(ICON_BTSP_FOLDER " Open Recent"))
            { /* Do stuff */
            }
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_BTSP_FOLDERSYMLINK " Save", "Ctrl+S"))
            { /* Do stuff */
            }
            if(ImGui::MenuItem(ICON_BTSP_FOLDERSYMLINK " Save As...", "Ctrl+Shift+S"))
            { /* Do stuff */
            }
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_BTSP_FOLDERDASH " Close Project", "Ctrl+W"))
            {
            }
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
            if(ImGui::MenuItem("Create Empty"))
            { /* Do stuff */
            }
            if(ImGui::BeginMenu("Create Geometry"))
            {
                if(ImGui::MenuItem("Cube"))
                { /* Do stuff */
                }
                if(ImGui::MenuItem("Sphere"))
                { /* Do stuff */
                }
                if(ImGui::MenuItem("Capsule"))
                { /* Do stuff */
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Component"))
        {
            if(ImGui::BeginMenu("Enable Component"))
            {
                if(ImGui::MenuItem("Camera"))
                { /* Do stuff */
                }
                if(ImGui::MenuItem("Transform"))
                { /* Do stuff */
                }
                if(ImGui::MenuItem("RigidBody"))
                { /* Do stuff */
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x * 0.5f - 150);

        ImGui::Text("%s", "untitled");

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 350);

        ImGui::BeginTabBar("##GamePlayTabs");

        // for(auto &tabPair: m_tabIds)
        // {
        //     if(ImGui::BeginTabItem(tabPair.first))
        //     {
        //         m_activeTab = tabPair.second;
        //         ImGui::EndTabItem();
        //     }
        // }

        ImGui::EndTabBar();

        ImGui::EndMenuBar();
    }
}