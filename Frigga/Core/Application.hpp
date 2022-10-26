#pragma once

#include <vector>

#include "Core/LayerStack.hpp"
#include "Core/Window.hpp"
#include "Gui/GuiLayer.hpp"
#include "Scene/Scene.hpp"

FRIGGA_BEGIN

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

class Application
{
  public:
    ~Application();

    static Application *Get();

    void onEvent(Event &event);

    std::string getName()
    {
        return m_name;
    }

    static void PushLayer(Layer *layer)
    {
        Get()->pushLayer(layer);
    }
    static void PushOverlay(Layer *layer)
    {
        Get()->pushOverlay(layer);
    }

    void run();

    static shared<Window> GetWindow()
    {
        return Get()->getWindow();
    }

  protected:
    Application(std::string name, uint width, uint height, std::vector<std::string> icons = {});

    void pushLayer(Layer *layer);
    void pushOverlay(Layer *layer);

    shared<Window> getWindow(int index = 0)
    {
        return m_windows[index];
    };

    std::vector<shared<Window>> m_windows;
    LayerStack m_layerStack;
    shared<Scene> m_scene;
    unique<GuiLayer> m_GuiLayer;

    std::string m_name;
    uint m_width;
    uint m_height;
    std::vector<std::string> m_icons;
};

Application *CreateApplication();

FRIGGA_END
