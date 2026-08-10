#pragma once

#include "Backends/imgui_impl_sdl3.h"
#include <Freya/Vulkan.hpp>

#include "Frigga/Core/Layer.hpp"

namespace FRIGGA_NAMESPACE
{

    class GuiLayer: public Layer
    {
      public:
        GuiLayer(skr::Arc<skr::ServiceProvider> serviceProvider)
            : fg::Layer("GuiLayer"), mServiceProvider(serviceProvider)
        {
        }

        ~GuiLayer() = default;

        virtual void onAttach() override;
        virtual void onDettach() override;
        virtual void onEvent(Event &event) override;

        void begin();
        void end();

        /// Recreate the ImGui Vulkan main pipeline after Freya rebuilds the UI
        /// render pass (swapchain/VSync, resize, …).
        static void RecreateMainPipeline(const skr::Arc<fra::Renderer> &renderer);

        void setBlockEvents(bool block)
        {
            m_blockEvents = block;
        }

      private:
        void configureStyle();

        bool m_blockEvents             = true;
        bool mEventCallbackRegistered  = false;
        float m_time                   = 0.9f;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
    };

} // namespace FRIGGA_NAMESPACE
