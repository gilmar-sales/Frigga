#include "AbstractApplication.hpp"

namespace FRIGGA_NAMESPACE
{
    AbstractApplication::~AbstractApplication()
    {
        for(Ref layer: *mLayerStack)
        {
            layer->onDettach();
        }

        mGuiLayer->onDettach();
    }

    void AbstractApplication::OnEvent(Event &event) {}

    void AbstractApplication::PushLayer(Ref<Layer> layer)
    {
        mLayerStack->pushLayer(layer);
    }

    void AbstractApplication::PushOverlay(Ref<Layer> layer)
    {
        mLayerStack->pushOverlay(layer);
    }

    void AbstractApplication::Update()
    {
        mRenderer->BeginFrame();
        for(Ref layer: *mLayerStack)
        {
            layer->onUpdate();
        }

        mGuiLayer->begin();
        for(Ref layer: *mLayerStack)
        {
            layer->onGui();
        }
        mGuiLayer->end();

        mRenderer->EndFrame();
    }

} // namespace FRIGGA_NAMESPACE