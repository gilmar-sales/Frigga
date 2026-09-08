#pragma once

#include "Backends/imgui_impl_sdl3.h"
#include "Backends/imgui_impl_vulkan.h"
#include <Freya/Freya.hpp>

#include "Frigga/Core/Layer.hpp"

#include <memory>

namespace FRIGGA_NAMESPACE
{

    struct PendingImGuiSdlEvents;

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
        /// render pass (swapchain/VSync, resize, …). Waits for the GPU so the
        /// previous pipeline is not destroyed while still referenced.
        static void RecreateMainPipeline(const skr::Arc<fra::Renderer> &renderer);

        /// Mark the ImGui main pipeline dirty; flushed in end() after Freya
        /// BeginFrame may have rebuilt the UI render pass (window resize).
        static void RequestRecreateMainPipeline();

        void setBlockEvents(bool block)
        {
            m_blockEvents = block;
        }

      private:
        void configureStyle();
        void flushPendingPipelineRecreate();

        bool m_blockEvents             = true;
        bool mEventCallbackRegistered  = false;
        float m_time                   = 0.9f;
        skr::Arc<skr::ServiceProvider>                  mServiceProvider;
        skr::Arc<fra::Renderer>                         mRenderer;
        std::shared_ptr<PendingImGuiSdlEvents>          mPendingSdlEvents;
    };

} // namespace FRIGGA_NAMESPACE
