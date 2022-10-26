#include "RendererAPI.hpp"

#include "Core/Log.hpp"
#include "Platform/OpenGL/OpenGLRendererAPI.hpp"

FRIGGA_BEGIN

RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;

shared<RendererAPI> RendererAPI::Create()
{
    switch(s_API)
    {
    case RendererAPI::API::None:
        FG_LOG_ERROR("RendererAPI::None is currently not supported!");
        return nullptr;
    case RendererAPI::API::OpenGL:
        return std::make_shared<OpenGLRendererAPI>();
    }

    FG_LOG_ERROR("Unknown RendererAPI!");
    return nullptr;
}

FRIGGA_END