#include "GuiLayer.hpp"

#include "Styles/Styles.hpp"

#include "Backends/imgui_impl_sdl3.h"
#include "Backends/imgui_impl_vulkan.h"

namespace FRIGGA_NAMESPACE
{

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

        auto native_window = mServiceProvider->GetService<fra::Window>()->Get();

        auto imguiSdl3VulkanInitInfo     = ImGui_ImplVulkan_InitInfo{};
        imguiSdl3VulkanInitInfo.Instance = mServiceProvider->GetService<fra::Instance>()->Get();
        imguiSdl3VulkanInitInfo.PhysicalDevice =
            mServiceProvider->GetService<fra::PhysicalDevice>()->Get();
        imguiSdl3VulkanInitInfo.Device = mServiceProvider->GetService<fra::Device>()->Get();
        imguiSdl3VulkanInitInfo.Queue =
            mServiceProvider->GetService<fra::Device>()->GetGraphicsQueue();
        imguiSdl3VulkanInitInfo.ImageCount         = 4;
        imguiSdl3VulkanInitInfo.MinImageCount      = 2;
        imguiSdl3VulkanInitInfo.DescriptorPoolSize = 32;
        imguiSdl3VulkanInitInfo.PipelineInfoMain.RenderPass =
            mServiceProvider->GetService<fra::RenderPass>()->Get();

        ImGui_ImplSDL3_InitForVulkan(native_window);
        ImGui_ImplVulkan_Init(&imguiSdl3VulkanInitInfo);
    }

    void GuiLayer::onDettach()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
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
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void GuiLayer::end()
    {
        auto window = mServiceProvider->GetService<fra::Window>();

        ImGuiIO &io    = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)window->GetWidth(), (float)window->GetHeight());

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(
            ImGui::GetDrawData(),
            mServiceProvider->GetService<fra::CommandPool>()->GetCommandBuffer());

        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
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

} // namespace FRIGGA_NAMESPACE
