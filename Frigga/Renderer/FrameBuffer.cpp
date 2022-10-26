#include "FrameBuffer.hpp"

#include "Core/Log.hpp"
#include "Renderer/Renderer.hpp"

#include "Platform/OpenGL/OpenGLFrameBuffer.hpp"

FRIGGA_BEGIN

shared<FrameBuffer> FrameBuffer::create(const FrameBufferSpecification &spec)
{
    switch(Renderer::GetAPI())
    {
    case RendererAPI::API::None:
        FG_LOG_ERROR("RendererAPI::None is currently not supported!");
        return nullptr;
    case RendererAPI::API::OpenGL:
        return std::make_shared<OpenGLFrameBuffer>(spec);
    default:
        FG_LOG_ERROR("Unknown RendererAPI!");
    }

    return nullptr;
}

FRIGGA_END
