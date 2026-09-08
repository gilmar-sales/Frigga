#pragma once

#include <vector>

#include "Freya/Core/AbstractApplication.hpp"
#include "Frigga/Core/LayerStack.hpp"
#include "Frigga/Gui/GuiLayer.hpp"
#include "Frigga/Scene/Scene.hpp"

namespace FRIGGA_NAMESPACE
{
#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

    class AbstractApplication: public fra::AbstractApplication
    {
      public:
        AbstractApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider);

        ~AbstractApplication();

        void Update() override;

        void OnEvent(Event &event);

        void PushLayer(skr::Arc<Layer> layer);
        void PushOverlay(skr::Arc<Layer> layer);

      protected:
        /// Called between BeginFrame and EndScene to queue scene draw commands.
        virtual void RenderScene() {}

        /// Called after onGuiBegin and before BeginFrame (editor: apply viewport claims).
        virtual void OnAfterGuiLayout() {}

        /// Called after Renderer::BeginFrame (editor: reassert panel RT if swapchain rebuild resized it).
        virtual void OnAfterBeginFrame() {}

        /// When false, skip bootstrapping a full-window viewport if the offscreen image is invalid.
        [[nodiscard]] virtual bool ShouldBootstrapViewportFallback() const
        {
            return true;
        }

        /// Bind Frigga UI / LayerStack to Freya's main window scope (not a sibling).
        void createScope();

        /// Construct Freya-bound singletons from the main window provider so later
        /// root/Freyr lookups hit the cache instead of an unseeded scope.
        void warmFreyaBoundSingletons();

        skr::Arc<skr::ServiceScope> mScope;
        skr::Arc<GuiLayer>          mGuiLayer;
    };

} // namespace FRIGGA_NAMESPACE