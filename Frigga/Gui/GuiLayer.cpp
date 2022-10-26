#include "Gui/GuiLayer.hpp"

#include <GLFW/glfw3.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>

#include "Core/Application.hpp"

#include "Gui/Styles/Styles.hpp"

FRIGGA_BEGIN

GuiLayer::GuiLayer(shared<Window> window): Layer("GuiLayer"), window(window) {}

void GuiLayer::onAttach()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

    configureStyle();

    StylePhantomDark();

    auto *native_window = (GLFWwindow *)(window->getNativeWindow());

    ImGui_ImplGlfw_InitForOpenGL(native_window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
}

void GuiLayer::onDettach()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GuiLayer::onEvent(Event &event)
{
    if(m_blockEvents)
    {
        ImGuiIO &io = ImGui::GetIO();
        event.Handled |= event.isInCategory(EventCategoryMouse) & io.WantCaptureMouse;
        event.Handled |= event.isInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
    }
}

void GuiLayer::begin()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GuiLayer::end()
{
    ImGuiIO &io    = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)Application::GetWindow()->getWidth(), (float)Application::GetWindow()->getHeight());

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow *backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void GuiLayer::configureStyle()
{
    ImGuiStyle &style = ImGui::GetStyle();

    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    style.WindowRounding   = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding    = {5.0f, 5.0f};

    style.GrabRounding = 0.0f;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
    style.IndentSpacing    = 22;

    style.ChildRounding   = 0.0f;
    style.ChildBorderSize = 0.0f;

    style.ScrollbarRounding = 0.0f;
    style.ScrollbarSize     = 16;

    style.TabRounding   = 0.0f;
    style.TabBorderSize = 1.0f;

    style.FrameRounding   = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.FramePadding    = {6.0f, 4.0f};

    style.PopupBorderSize = 0.0f;
    style.PopupRounding   = 0.0f;

    style.ItemInnerSpacing = ImVec2(6, 6);
    style.ItemSpacing      = ImVec2(6, 8);

    style.Alpha                    = 1.0f;
    style.WindowMenuButtonPosition = ImGuiDir_None;
}

FRIGGA_END
