#include <Frigga/Gui/GuiLayer.hpp>

#include <Frigga/Gui/Styles/Styles.hpp>

#include <Frigga/Gui/Backends/imgui_impl_sdl3.h>
#include <Frigga/Gui/Backends/imgui_impl_vulkan.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>
#include <vector>

#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Gamepad.hpp>
#include <Freya/Events/Keyboard.hpp>
#include <Freya/Events/Mouse.hpp>
#include <Freya/Events/Window.hpp>
#include <Freya/FreyaOptions.hpp>

namespace FRIGGA_NAMESPACE
{
    struct PendingImGuiSdlEvents
    {
        std::vector<SDL_Event> events;
    };

    namespace
    {
        struct KeyModState
        {
            bool shift    = false;
            bool ctrl     = false;
            bool alt      = false;
            bool capsLock = false;
        };

        /// Freya's Window::pollEvents drops SDL wheel/text events before ImGui
        /// sees them. Capture via SDL_AddEventWatch and flush in begin().
        bool SDLCALL CaptureDroppedSdlEvents(void *userdata, SDL_Event *event)
        {
            if(event == nullptr || userdata == nullptr)
            {
                return true;
            }

            switch(event->type)
            {
            case SDL_EVENT_MOUSE_WHEEL:
            case SDL_EVENT_TEXT_INPUT:
                static_cast<PendingImGuiSdlEvents *>(userdata)->events.push_back(*event);
                break;
            default:
                break;
            }

            return true;
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
        }

        void DispatchImGuiMouseButton(const fra::MouseButton button, const bool down,
                                      const SDL_WindowID windowId)
        {
            SDL_Event ev {};
            ev.type            = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
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
            ev.type            = SDL_EVENT_MOUSE_MOTION;
            ev.motion.windowID = windowId;
            ev.motion.x        = event.x;
            ev.motion.y        = event.y;
            ev.motion.xrel     = event.deltaX;
            ev.motion.yrel     = event.deltaY;
            ev.motion.which    = 0;
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

        mRenderer = mServiceProvider->GetService<fra::Renderer>();
        const fra::ImGuiNativeHandles native = mRenderer->GetImGuiNativeHandles();

        auto sdlWindow              = static_cast<SDL_Window *>(native.window);
        const SDL_WindowID windowId = SDL_GetWindowID(sdlWindow);

        ImGui_ImplSDL3_InitForVulkan(sdlWindow);

        std::uint32_t imageCount = native.minImageCount;
        if(const auto options = mServiceProvider->GetService<fra::FreyaOptions>())
        {
            imageCount = std::max(imageCount, options->frameCount);
        }
        imageCount = std::max(imageCount, mRenderer->GetFrameCount());
        imageCount = std::max(imageCount, 2u);

        auto imguiSdl3VulkanInitInfo     = ImGui_ImplVulkan_InitInfo{};
        imguiSdl3VulkanInitInfo.Instance = static_cast<VkInstance>(native.instance);
        imguiSdl3VulkanInitInfo.PhysicalDevice =
            static_cast<VkPhysicalDevice>(native.physicalDevice);
        imguiSdl3VulkanInitInfo.Device = static_cast<VkDevice>(native.device);
        imguiSdl3VulkanInitInfo.Queue =
            static_cast<VkQueue>(native.graphicsQueue);
        imguiSdl3VulkanInitInfo.ImageCount         = imageCount;
        imguiSdl3VulkanInitInfo.MinImageCount      = native.minImageCount;
        imguiSdl3VulkanInitInfo.DescriptorPoolSize = 64;
        imguiSdl3VulkanInitInfo.PipelineInfoMain.RenderPass =
            static_cast<VkRenderPass>(native.renderPass);

        ImGui_ImplVulkan_Init(&imguiSdl3VulkanInitInfo);

        if(mPendingSdlEvents == nullptr)
        {
            mPendingSdlEvents = std::make_shared<PendingImGuiSdlEvents>();
            SDL_AddEventWatch(CaptureDroppedSdlEvents, mPendingSdlEvents.get());
        }

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
                events->Subscribe<fra::WindowResizeEvent>(
                    [renderer = mRenderer](const fra::WindowResizeEvent &) {
                        RecreateMainPipeline(renderer);
                    });
            }
            mEventCallbackRegistered = true;
        }
    }

    void GuiLayer::onDettach()
    {
        if(mPendingSdlEvents != nullptr)
        {
            SDL_RemoveEventWatch(CaptureDroppedSdlEvents, mPendingSdlEvents.get());
            mPendingSdlEvents.reset();
        }

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
        if(mPendingSdlEvents != nullptr)
        {
            for(const SDL_Event &event : mPendingSdlEvents->events)
            {
                ImGui_ImplSDL3_ProcessEvent(&event);
            }
            mPendingSdlEvents->events.clear();
        }

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

        ImDrawData *drawData = ImGui::GetDrawData();
        if(drawData != nullptr && drawData->TotalVtxCount > 0)
        {
            if(renderer->BeginUI())
            {
                ImGui_ImplVulkan_RenderDrawData(
                    drawData,
                    static_cast<VkCommandBuffer>(renderer->NativeCommandBuffer()));
                renderer->EndUI();
            }
        }

        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void GuiLayer::configureStyle()
    {
        StyleModernMetrics();
    }

} // namespace FRIGGA_NAMESPACE
