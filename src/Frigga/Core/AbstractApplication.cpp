#include "AbstractApplication.hpp"

namespace FRIGGA_NAMESPACE
{
    AbstractApplication::~AbstractApplication()
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        for(const auto &layer : *layerStack)
        {
            layer->onDettach();
        }
    }

    void AbstractApplication::OnEvent(Event &event) {}

    void AbstractApplication::PushLayer(skr::Arc<Layer> layer)
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        layerStack->pushLayer(layer);
    }

    void AbstractApplication::PushOverlay(skr::Arc<Layer> layer)
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        layerStack->pushOverlay(layer);
    }

    void AbstractApplication::Update()
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();

        // Pre-frame work (e.g. resize offscreen targets) must run before
        // BeginFrame — SetOutputTarget waits idle and rebuilds pass resources.
        for(const auto &layer : *layerStack)
        {
            layer->onUpdate();
        }

        mRenderer->BeginFrame();

        RenderScene();

        // v0.42 only hosts ImGui in the swapchain UI pass that follows an
        // offscreen composite. If no editor viewport panel claimed the renderer
        // output this frame (e.g. the home screen), fall back to a window-sized
        // viewport target so that pass is always opened and ImGui can render.
        if(!mRenderer->GetViewportImage().valid && mWindow)
        {
            mRenderer->SetViewportTarget(mWindow->GetWidth(), mWindow->GetHeight());
        }

        // End scene (composite → output target) and leave the swapchain UI pass
        // open when an offscreen target is bound so ImGui can record into it.
        mRenderer->EndScene();

        auto guiLayer = mGuiLayer;
        if(guiLayer == nullptr)
        {
            guiLayer = mScope->GetServiceProvider()->GetService<GuiLayer>();
        }
        guiLayer->begin();
        for(const auto &layer : *layerStack)
        {
            layer->onGui();
        }
        guiLayer->end();

        mRenderer->Present();
    }

} // namespace FRIGGA_NAMESPACE
