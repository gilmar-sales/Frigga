#include "OpenGLContext.hpp"

#include "Core/Log.hpp"

using namespace gl;

FRIGGA_BEGIN

OpenGLContext::OpenGLContext(GLFWwindow *windowHandle): m_windowHandle(windowHandle)
{
    FG_ASSERT(windowHandle);
}

void OpenGLContext::init()
{
    glfwMakeContextCurrent(m_windowHandle);
    glbinding::initialize(glfwGetProcAddress);

    if(glGetString(GL_VERSION) == nullptr)
        FG_LOG_ERROR("Couldn't initialize OpenGL!");
    else
        FG_LOG_TRACE("OpenGL initialized!");

    FG_LOG_INFO("OpenGL Info:");
    FG_LOG_INFO("\tVendor: {}", (char *)glGetString(GL_VENDOR));
    FG_LOG_INFO("\tRenderer: {}", (char *)glGetString(GL_RENDERER));
    FG_LOG_INFO("\tVersion: {}", (char *)glGetString(GL_VERSION));
}

void OpenGLContext::swapBuffers()
{
    glfwSwapBuffers(m_windowHandle);
}

FRIGGA_END
