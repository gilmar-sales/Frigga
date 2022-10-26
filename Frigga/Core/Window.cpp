#include "Core/Window.hpp"

#include "Core/Log.hpp"
#include "Renderer/RendererAPI.hpp"

#include "vendor/stb_image.h"

FRIGGA_BEGIN

uint Window::WindowCount = 0;

static void GlfwErrorCallback(int error, const char *description)
{
    FG_LOG_ERROR("[glfw] ({}): {}", error, description);
}

Window::Window(std::string title, uint width, uint height, std::vector<std::string> icons)
    : m_data({title, width, height, icons, {}, false, false})
{
    init();
}

Window::~Window()
{
    glfwDestroyWindow(m_nativeWindow);

    glfwTerminate();
}

void Window::update()
{
    m_renderContext->swapBuffers();

    glfwPollEvents();
}

void Window::close()
{
    glfwSetWindowShouldClose(m_nativeWindow, 1);
    glfwPostEmptyEvent();
}

void Window::init()
{
    if(!WindowCount)
    {
        if(!glfwInit())
            FG_LOG_ERROR("Couldn't initialize GLFW!");
        else
        {
            FG_LOG_TRACE("GLFW initialized!");
        }

        glfwSetErrorCallback(GlfwErrorCallback);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

    WindowCount++;

    glfwWindowHint(GLFW_MAXIMIZED, m_data.maximized);

    m_nativeWindow = glfwCreateWindow(m_data.width, m_data.height, m_data.title.c_str(), nullptr, nullptr);
    setIcon(m_data.icons);

    m_renderContext = RenderContext::create(m_nativeWindow);
    m_renderContext->init();

    glfwSetWindowUserPointer(m_nativeWindow, &m_data);
    glfwSwapInterval(0);

    RendererAPI::SetClearColor({0.2f, 0.4f, 0.6f, 0.8f});

    glfwSetWindowSizeCallback(m_nativeWindow, [](GLFWwindow *window, int width, int height) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);
        data.width       = width;
        data.height      = height;

        WindowResizeEvent event(width, height);
        data.eventCallback(event);
    });

    glfwSetWindowCloseCallback(m_nativeWindow, [](GLFWwindow *window) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);
        WindowCloseEvent event;
        data.eventCallback(event);
    });

    glfwSetKeyCallback(m_nativeWindow, [](GLFWwindow *window, int key, int scancode, int action, int modes) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        switch(action)
        {
        case GLFW_PRESS: {
            KeyPressedEvent event(key, 0);
            data.eventCallback(event);
            break;
        }
        case GLFW_RELEASE: {
            KeyReleasedEvent event(key);
            data.eventCallback(event);
            break;
        }
        case GLFW_REPEAT: {
            KeyPressedEvent event(key, 1);
            break;
        }
        }
    });

    glfwSetCharCallback(m_nativeWindow, [](GLFWwindow *window, unsigned int key) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        KeyTypedEvent event(key);
        data.eventCallback(event);
    });

    glfwSetMouseButtonCallback(m_nativeWindow, [](GLFWwindow *window, int button, int action, int mods) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        switch(action)
        {
        case GLFW_PRESS: {
            MouseButtonPressedEvent event(button);
            data.eventCallback(event);
            break;
        }
        case GLFW_RELEASE: {
            MouseButtonReleasedEvent event(button);
            data.eventCallback(event);
            break;
        }
        }
    });

    glfwSetScrollCallback(m_nativeWindow, [](GLFWwindow *window, double xOffset, double yOffset) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        MouseScrolledEvent event((float)xOffset, (float)yOffset);
        data.eventCallback(event);
    });

    glfwSetCursorPosCallback(m_nativeWindow, [](GLFWwindow *window, double x, double y) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        float fx = floor(x);
        float fy = floor(y);
        MouseMoveEvent event(fx, fy);
        data.eventCallback(event);
        data.mousePos = {fx, fy};
    });

    glfwSetWindowMaximizeCallback(m_nativeWindow, [](GLFWwindow *window, int maximized) {
        WindowData *data = (WindowData *)glfwGetWindowUserPointer(window);

        data->maximized = maximized;
    });
}

void Window::setIcon(std::vector<std::string> paths)
{
    int channels;
    std::vector<GLFWimage> icons = std::vector<GLFWimage>(paths.size());

    for(int i = 0; i < paths.size(); i++)
    {
        icons[i].pixels = stbi_load(paths[i].c_str(), &icons[i].width, &icons[i].height, &channels, STBI_rgb_alpha);

        if(icons[i].pixels == nullptr)
        {
            FG_LOG_TRACE("Failed to load icon from {}", paths[i]);
        }
    }

    if(!icons.empty()) glfwSetWindowIcon(m_nativeWindow, (int)icons.size(), &icons[0]);

    for(auto &icon: icons)
    {
        stbi_image_free(icon.pixels);
    }
}

FRIGGA_END
