#pragma once

#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include "Events/ApplicationEvent.hpp"
#include "Events/KeyEvent.hpp"
#include "Events/MouseEvent.hpp"
#include "Renderer/RenderContext.hpp"

FRIGGA_BEGIN

class Window
{
    using EventCallbackFn = std::function<void(Event &)>;

    struct WindowData
    {
        std::string title;
        uint width;
        uint height;
        std::vector<std::string> icons;
        glm::vec2 mousePos;
        bool maximized;
        bool vsync;
        EventCallbackFn eventCallback;
    };

  public:
    Window(std::string title, uint width, uint height, std::vector<std::string> icons);
    ~Window();

    void update();

    void close();

    bool shouldClose()
    {
        return glfwWindowShouldClose(m_nativeWindow);
    }

    void *getNativeWindow()
    {
        return m_nativeWindow;
    }
    uint getWidth()
    {
        return m_data.width;
    }
    uint getHeight()
    {
        return m_data.height;
    }
    glm::vec2 getMousePos()
    {
        return m_data.mousePos;
    }

    bool isMaximized()
    {
        return m_data.maximized;
    }

    void setEventCallback(const EventCallbackFn &callback)
    {
        m_data.eventCallback = callback;
    }

    void setIcon(std::vector<std::string> paths);

    void setVSync(bool value)
    {
        if(value)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        m_data.vsync = value;
    }

  private:
    void init();

    static uint WindowCount;

    GLFWwindow *m_nativeWindow;
    unique<RenderContext> m_renderContext;
    WindowData m_data;
};

FRIGGA_END