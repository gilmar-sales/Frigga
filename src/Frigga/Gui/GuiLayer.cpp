#include "GuiLayer.hpp"

#include "Styles/Styles.hpp"

#include "Backends/imgui_impl_sdl3.h"
#include "Backends/imgui_impl_vulkan.h"

#include <SDL3/SDL.h>

#include <memory>

#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Gamepad.hpp>
#include <Freya/Events/Keyboard.hpp>
#include <Freya/Events/Mouse.hpp>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        struct KeyModState
        {
            bool shift    = false;
            bool ctrl     = false;
            bool alt      = false;
            bool capsLock = false;
        };

        /// Unshifted / shifted characters for SDL scancode -> typable text.
        /// Keys whose scancode is not listed here do not produce text input.
        char PrintableChar(const KeyModState &mods, const SDL_Scancode scancode)
        {
            char base     = '\0';
            char shifted  = '\0';
            switch(scancode)
            {
            case SDL_SCANCODE_A: base = 'a'; shifted = 'A'; break;
            case SDL_SCANCODE_B: base = 'b'; shifted = 'B'; break;
            case SDL_SCANCODE_C: base = 'c'; shifted = 'C'; break;
            case SDL_SCANCODE_D: base = 'd'; shifted = 'D'; break;
            case SDL_SCANCODE_E: base = 'e'; shifted = 'E'; break;
            case SDL_SCANCODE_F: base = 'f'; shifted = 'F'; break;
            case SDL_SCANCODE_G: base = 'g'; shifted = 'G'; break;
            case SDL_SCANCODE_H: base = 'h'; shifted = 'H'; break;
            case SDL_SCANCODE_I: base = 'i'; shifted = 'I'; break;
            case SDL_SCANCODE_J: base = 'j'; shifted = 'J'; break;
            case SDL_SCANCODE_K: base = 'k'; shifted = 'K'; break;
            case SDL_SCANCODE_L: base = 'l'; shifted = 'L'; break;
            case SDL_SCANCODE_M: base = 'm'; shifted = 'M'; break;
            case SDL_SCANCODE_N: base = 'n'; shifted = 'N'; break;
            case SDL_SCANCODE_O: base = 'o'; shifted = 'O'; break;
            case SDL_SCANCODE_P: base = 'p'; shifted = 'P'; break;
            case SDL_SCANCODE_Q: base = 'q'; shifted = 'Q'; break;
            case SDL_SCANCODE_R: base = 'r'; shifted = 'R'; break;
            case SDL_SCANCODE_S: base = 's'; shifted = 'S'; break;
            case SDL_SCANCODE_T: base = 't'; shifted = 'T'; break;
            case SDL_SCANCODE_U: base = 'u'; shifted = 'U'; break;
            case SDL_SCANCODE_V: base = 'v'; shifted = 'V'; break;
            case SDL_SCANCODE_W: base = 'w'; shifted = 'W'; break;
            case SDL_SCANCODE_X: base = 'x'; shifted = 'X'; break;
            case SDL_SCANCODE_Y: base = 'y'; shifted = 'Y'; break;
            case SDL_SCANCODE_Z: base = 'z'; shifted = 'Z'; break;
            case SDL_SCANCODE_1: base = '1'; shifted = '!'; break;
            case SDL_SCANCODE_2: base = '2'; shifted = '@'; break;
            case SDL_SCANCODE_3: base = '3'; shifted = '#'; break;
            case SDL_SCANCODE_4: base = '4'; shifted = '$'; break;
            case SDL_SCANCODE_5: base = '5'; shifted = '%'; break;
            case SDL_SCANCODE_6: base = '6'; shifted = '^'; break;
            case SDL_SCANCODE_7: base = '7'; shifted = '&'; break;
            case SDL_SCANCODE_8: base = '8'; shifted = '*'; break;
            case SDL_SCANCODE_9: base = '9'; shifted = '('; break;
            case SDL_SCANCODE_0: base = '0'; shifted = ')'; break;
            case SDL_SCANCODE_MINUS: base = '-'; shifted = '_'; break;
            case SDL_SCANCODE_EQUALS: base = '='; shifted = '+'; break;
            case SDL_SCANCODE_LEFTBRACKET: base = '['; shifted = '{'; break;
            case SDL_SCANCODE_RIGHTBRACKET: base = ']'; shifted = '}'; break;
            case SDL_SCANCODE_BACKSLASH: base = '\\'; shifted = '|'; break;
            case SDL_SCANCODE_SEMICOLON: base = ';'; shifted = ':'; break;
            case SDL_SCANCODE_APOSTROPHE: base = '\''; shifted = '"'; break;
            case SDL_SCANCODE_GRAVE: base = '`'; shifted = '~'; break;
            case SDL_SCANCODE_COMMA: base = ','; shifted = '<'; break;
            case SDL_SCANCODE_PERIOD: base = '.'; shifted = '>'; break;
            case SDL_SCANCODE_SLASH: base = '/'; shifted = '?'; break;
            case SDL_SCANCODE_SPACE: base = ' '; shifted = ' '; break;
            default: return '\0';
            }

            if(mods.shift || mods.capsLock)
            {
                return shifted;
            }
            return base;
        }

        void DispatchImGuiKey(const fra::KeyCode key, const bool down,
                              const SDL_WindowID windowId, KeyModState &mods)
        {
            const auto scancode = static_cast<SDL_Scancode>(key);

            if(key == fra::KeyCode::LShift || key == fra::KeyCode::RShift)
            {
                mods.shift = down;
            }
            else if(key == fra::KeyCode::LCtrl || key == fra::KeyCode::RCtrl)
            {
                mods.ctrl = down;
            }
            else if(key == fra::KeyCode::LAlt || key == fra::KeyCode::RAlt)
            {
                mods.alt = down;
            }
            else if(key == fra::KeyCode::CapsLock && down)
            {
                mods.capsLock = !mods.capsLock;
            }

            const auto keymod = static_cast<SDL_Keymod>(
                (mods.shift ? SDL_KMOD_SHIFT : SDL_KMOD_NONE) |
                (mods.ctrl ? SDL_KMOD_CTRL : SDL_KMOD_NONE) |
                (mods.alt ? SDL_KMOD_ALT : SDL_KMOD_NONE));

            SDL_Event ev {};
            ev.type         = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            ev.key.windowID = windowId;
            ev.key.scancode = scancode;
            ev.key.key      = SDL_GetKeyFromScancode(scancode, keymod, false);
            ev.key.mod      = keymod;
            ev.key.repeat   = false;
            ev.key.down     = down;
            ev.key.which    = 0;
            ImGui_ImplSDL3_ProcessEvent(&ev);

            // Typed text requires SDL_EVENT_TEXT_INPUT, which Freya's event
            // system drops. Recover printable characters from the scancode +
            // tracked modifiers and feed them straight into ImGui.
            if(down && !mods.ctrl && !mods.alt && ImGui::GetCurrentContext() != nullptr)
            {
                const char ch = PrintableChar(mods, scancode);
                if(ch != '\0')
                {
                    ImGui::GetIO().AddInputCharacter(static_cast<ImWchar>(ch));
                }
            }
        }

        void DispatchImGuiMouseButton(const fra::MouseButton button, const bool down,
                                      const SDL_WindowID windowId)
        {
            SDL_Event ev {};
            ev.type          = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            ev.button.windowID = windowId;
            ev.button.button   = static_cast<std::uint8_t>(button);
            ev.button.down     = down;
            ev.button.clicks   = 1;
            ev.button.which    = 0;
            ImGui_ImplSDL3_ProcessEvent(&ev);
        }

        void DispatchImGuiMouseMove(const fra::MouseMoveEvent &event,
                                    const SDL_WindowID windowId)
        {
            SDL_Event ev {};
            ev.type         = SDL_EVENT_MOUSE_MOTION;
            ev.motion.windowID = windowId;
            ev.motion.x     = event.x;
            ev.motion.y     = event.y;
            ev.motion.xrel  = event.deltaX;
            ev.motion.yrel  = event.deltaY;
            ev.motion.which = 0;
            ImGui_ImplSDL3_ProcessEvent(&ev);
        }
    } // namespace

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

        auto renderer = mServiceProvider->GetService<fra::Renderer>();
        const fra::ImGuiNativeHandles native = renderer->GetImGuiNativeHandles();

        auto sdlWindow            = static_cast<SDL_Window *>(native.window);
        const SDL_WindowID windowId = SDL_GetWindowID(sdlWindow);

        ImGui_ImplSDL3_InitForVulkan(sdlWindow);

        auto imguiSdl3VulkanInitInfo     = ImGui_ImplVulkan_InitInfo{};
        imguiSdl3VulkanInitInfo.Instance = static_cast<VkInstance>(native.instance);
        imguiSdl3VulkanInitInfo.PhysicalDevice =
            static_cast<VkPhysicalDevice>(native.physicalDevice);
        imguiSdl3VulkanInitInfo.Device = static_cast<VkDevice>(native.device);
        imguiSdl3VulkanInitInfo.Queue =
            static_cast<VkQueue>(native.graphicsQueue);
        imguiSdl3VulkanInitInfo.ImageCount         = static_cast<std::uint32_t>(4);
        imguiSdl3VulkanInitInfo.MinImageCount      = native.minImageCount;
        imguiSdl3VulkanInitInfo.DescriptorPoolSize = 32;
        imguiSdl3VulkanInitInfo.PipelineInfoMain.RenderPass =
            static_cast<VkRenderPass>(native.renderPass);

        ImGui_ImplVulkan_Init(&imguiSdl3VulkanInitInfo);

        if(!mEventCallbackRegistered)
        {
            if(auto events = mServiceProvider->GetService<fra::EventManager>())
            {
                auto mods = std::make_shared<KeyModState>();
                events->Subscribe<fra::KeyPressedEvent>(
                    [windowId, mods](const fra::KeyPressedEvent &event) {
                        DispatchImGuiKey(event.key, true, windowId, *mods);
                    });
                events->Subscribe<fra::KeyReleasedEvent>(
                    [windowId, mods](const fra::KeyReleasedEvent &event) {
                        DispatchImGuiKey(event.key, false, windowId, *mods);
                    });
                events->Subscribe<fra::MouseButtonPressedEvent>(
                    [windowId](const fra::MouseButtonPressedEvent &event) {
                        DispatchImGuiMouseButton(event.button, true, windowId);
                    });
                events->Subscribe<fra::MouseButtonReleasedEvent>(
                    [windowId](const fra::MouseButtonReleasedEvent &event) {
                        DispatchImGuiMouseButton(event.button, false, windowId);
                    });
                events->Subscribe<fra::MouseMoveEvent>(
                    [windowId](const fra::MouseMoveEvent &event) {
                        DispatchImGuiMouseMove(event, windowId);
                    });
            }
            mEventCallbackRegistered = true;
        }
    }

    void GuiLayer::onDettach()
    {
        if(!ImGui::GetCurrentContext())
        {
            return;
        }

        // ViewportsEnable requires an explicit destroy while the platform +
        // renderer backends are still alive (DestroyContext alone asserts).
        ImGui::DestroyPlatformWindows();
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

    void GuiLayer::RecreateMainPipeline(const skr::Arc<fra::Renderer> &renderer)
    {
        if(renderer == nullptr || ImGui::GetCurrentContext() == nullptr)
        {
            return;
        }

        const fra::ImGuiNativeHandles native = renderer->GetImGuiNativeHandles();
        ImGui_ImplVulkan_PipelineInfo pipelineInfo {};
        pipelineInfo.RenderPass = static_cast<VkRenderPass>(native.renderPass);
        ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
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
        auto renderer = mServiceProvider->GetService<fra::Renderer>();

        ImGuiIO &io    = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)window->GetWidth(), (float)window->GetHeight());

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(
            ImGui::GetDrawData(),
            static_cast<VkCommandBuffer>(renderer->NativeCommandBuffer()));

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
