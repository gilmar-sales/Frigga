#pragma once

#include <memory>

#include <glm/glm.hpp>

FRIGGA_BEGIN

class Renderer;

class RendererAPI
{
    friend class Renderer;

  public:
    enum class API
    {
        None   = 0,
        OpenGL = 1
    };

    static void Init()
    {
        Get()->init();
    }

    static void SetViewport(uint x, uint y, uint width, uint height)
    {
        Get()->setViewport(x, y, width, height);
    };

    static void SetClearColor(const glm::vec4 &color)
    {
        Get()->setClearColor(color);
    };

    static void Clear()
    {
        Get()->clear();
    }

    static void DrawIndexed(const uint vertexArrayObject, const uint vertexBufferObject, const uint elementBufferObject,
                            const uint indexCount = 0)
    {
        Get()->drawIndexed(vertexArrayObject, vertexBufferObject, elementBufferObject, indexCount);
    }

  protected:
    virtual void init()                                               = 0;
    virtual void setViewport(uint x, uint y, uint width, uint height) = 0;
    virtual void setClearColor(const glm::vec4 &color)                = 0;
    virtual void clear()                                              = 0;

    virtual void drawIndexed(const uint vertexArrayObject, const uint vertexBufferObject,
                             const uint elementBufferObject, const uint indexCount = 0) = 0;

    static shared<RendererAPI> Get()
    {
        static shared<RendererAPI> s_instance = Create();
        return s_instance;
    };

    static API GetAPI()
    {
        return s_API;
    }

    static shared<RendererAPI> Create();

  private:
    static API s_API;
};

FRIGGA_END
