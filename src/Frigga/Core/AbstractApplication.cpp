#include <Frigga/Core/AbstractApplication.hpp>

#include <Frigga/ECS/Systems/AnimationSystem.hpp>
#include <Frigga/ECS/Systems/RenderSystem.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Scene/Scene.hpp>

#include <Freya/Advanced.hpp>

namespace FRIGGA_NAMESPACE
{
    AbstractApplication::AbstractApplication(
        const skr::Arc<skr::ServiceProvider> &serviceProvider)
        : fra::AbstractApplication(serviceProvider)
    {
        createScope();
        warmFreyaBoundSingletons();
    }

    AbstractApplication::~AbstractApplication()
    {
        auto layerStack = mScope->GetServiceProvider()->GetService<LayerStack>();
        for(const auto &layer : *layerStack)
        {
            layer->onDettach();
        }
    }

    void AbstractApplication::createScope()
    {
        // Freya 0.44+: Window / Renderer / EventManager / FreyaOptions are scoped
        // to the main window. Nested Frigga services must share that scope.
        mScope = GetMainScope();

        mGuiLayer = mScope->GetServiceProvider()->GetService<GuiLayer>();
        PushLayer(mGuiLayer);
    }

    void AbstractApplication::warmFreyaBoundSingletons()
    {
        const auto sp = GetMainServiceProvider();
        if(!sp)
        {
            return;
        }

        (void)sp->GetService<Scene>();
        (void)sp->GetService<Input>();
        (void)sp->GetService<RenderSystem>();
        (void)sp->GetService<AnimationSystem>();
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

        if(!fra::Advanced(*mRenderer).GetViewportImage().valid && mWindow &&
           ShouldBootstrapViewportFallback())
        {
            (void)fra::Advanced(*mRenderer)
                .SetViewportTarget(mWindow->GetWidth(), mWindow->GetHeight());
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
