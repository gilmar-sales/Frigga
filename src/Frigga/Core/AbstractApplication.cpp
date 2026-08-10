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
