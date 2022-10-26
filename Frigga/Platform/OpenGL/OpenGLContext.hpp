#pragma once

#include <GLFW/glfw3.h>
#include <glbinding/gl/gl.h>
#include <glbinding/gl45core/gl.h>
#include <glbinding/gl45ext/gl.h>
#include <glbinding/glbinding.h>

#include "Renderer/RenderContext.hpp"

FRIGGA_BEGIN

class OpenGLContext: public RenderContext
{
  public:
    OpenGLContext(GLFWwindow *windowHandle);

    virtual void init() override;
    virtual void swapBuffers() override;

  private:
    GLFWwindow *m_windowHandle;
};

FRIGGA_END
