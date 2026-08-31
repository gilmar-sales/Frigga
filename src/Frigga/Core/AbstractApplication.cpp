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

        for(const auto &layer : *layerStack)
        {
            layer->onUpdate();
        }

        auto guiLayer = mGuiLayer;
        if(guiLayer == nullptr)
        {
            guiLayer = mScope->GetServiceProvider()->GetService<GuiLayer>();
        }

        guiLayer->begin();
        for(const auto &layer : *layerStack)
        {
            layer->onGuiBegin();
        }

        OnAfterGuiLayout();

        if(!mRenderer->GetViewportImage().valid && mWindow && ShouldBootstrapViewportFallback())
        {
            (void)mRenderer->SetViewportTarget(mWindow->GetWidth(), mWindow->GetHeight());
        }

        mRenderer->BeginFrame();

        RenderScene();

        mRenderer->EndScene();

        for(const auto &layer : *layerStack)
        {
            layer->onGuiEnd();
        }
        guiLayer->end();

        mRenderer->Present();
    }

} // namespace FRIGGA_NAMESPACE
