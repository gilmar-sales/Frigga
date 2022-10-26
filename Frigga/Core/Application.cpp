#include "Application.hpp"

#include <Renderer/RendererAPI.hpp>

FRIGGA_BEGIN

Application::Application(std::string name, uint width, uint height, std::vector<std::string> icons)
    : m_name(name), m_width(width), m_height(height), m_icons(icons), m_windows(0)
{
    FG_LOG_TRACE("Initializing {} application!", name);
    m_windows.push_back(std::make_shared<Window>(name, width, height, icons));

    getWindow()->setEventCallback(BIND_EVENT_FN(onEvent));

    m_scene      = std::make_shared<Scene>();
    m_layerStack = LayerStack();

    m_GuiLayer = std::make_unique<GuiLayer>(getWindow());
    pushLayer(&(*m_GuiLayer));

    FG_LOG_TRACE("{} initialized!", name);
};

Application::~Application()
{
    for(Layer *layer: m_layerStack)
    {
        layer->onDettach();
    }

    m_GuiLayer->onDettach();
}

Application *Application::Get()
{
    static Application *instance = CreateApplication();
    return instance;
};

void Application::onEvent(Event &event)
{
    FG_LOG_TRACE("[event]: {}", event.toString());
}

void Application::pushLayer(Layer *layer)
{
    m_layerStack.pushLayer(layer);
}

void Application::pushOverlay(Layer *layer)
{
    m_layerStack.pushOverlay(layer);
}

void Application::run()
{
    while(!m_windows[0]->shouldClose())
    {
        RendererAPI::Clear();

        // m_scene->update();

        for(Layer *layer: m_layerStack)
        {
            layer->onUpdate();
        }

        m_GuiLayer->begin();
        for(Layer *layer: m_layerStack)
        {
            layer->onGui();
        }
        m_GuiLayer->end();

        m_windows[0]->update();
    }
}

FRIGGA_END