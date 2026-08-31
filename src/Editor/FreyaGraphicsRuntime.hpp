#pragma once

#include <Freya/Freya.hpp>
#include <Skirnir/Skirnir.hpp>

#include <string>

namespace editor
{

    struct GraphicsReloadResult
    {
        bool rebuiltSwapChain = false;
        bool rebuiltIbl       = false;
    };

    GraphicsReloadResult ApplyShaderRoot(const skr::Arc<fra::Renderer> &renderer,
                                         const skr::Arc<fra::FreyaOptions> &options,
                                         std::string shaderRoot);

    GraphicsReloadResult ApplyEnvironmentMap(const skr::Arc<skr::ServiceProvider> &services,
                                             const skr::Arc<fra::Renderer> &renderer,
                                             const skr::Arc<fra::FreyaOptions> &options,
                                             std::string environmentMapPath);

    GraphicsReloadResult ApplyReverseZ(const skr::Arc<fra::Renderer> &renderer,
                                       const skr::Arc<fra::FreyaOptions> &options, bool reverseZ);

} // namespace editor
