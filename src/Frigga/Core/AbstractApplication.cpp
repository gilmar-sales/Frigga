#include "AbstractApplication.hpp"

namespace FRIGGA_NAMESPACE
{
    AbstractApplication::~AbstractApplication()
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        for(Ref layer: *layerStack)
        {
            layer->onDettach();
        }
    }

    void AbstractApplication::OnEvent(Event &event) {}

    void AbstractApplication::PushLayer(Ref<Layer> layer)
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        layerStack->pushLayer(layer);
    }

    void AbstractApplication::PushOverlay(Ref<Layer> layer)
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        layerStack->pushOverlay(layer);
    }

    void AbstractApplication::Update()
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        mRenderer->BeginFrame();
        for(Ref layer: *layerStack)
        {
            layer->onUpdate();
        }

        auto guiLayer = mScope->GetServiceProvider()->GetService<GuiLayer>();
        guiLayer->begin();
        for(Ref layer: *layerStack)
        {
            layer->onGui();
        }
        guiLayer->end();

        mRenderer->EndFrame();
    }

} // namespace FRIGGA_NAMESPACE