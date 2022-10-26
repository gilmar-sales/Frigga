#include "RenderContext.hpp"

#include "Core/Log.hpp"
#include "Platform/OpenGL/OpenGLContext.hpp"
#include "Renderer/Renderer.hpp"

FRIGGA_BEGIN

unique<RenderContext> RenderContext::create(void *window)
{
    switch(Renderer::GetAPI())
    {
    case RendererAPI::API::None:
        FG_LOG_ERROR("RendererAPI::None is currently not supported!");
        return nullptr;
    case RendererAPI::API::OpenGL:
        return std::make_unique<OpenGLContext>(static_cast<GLFWwindow *>(window));
    }

    FG_LOG_ERROR("Unknown RendererAPI!");
    return nullptr;
}

FRIGGA_END
