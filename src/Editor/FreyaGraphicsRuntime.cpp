#include "FreyaGraphicsRuntime.hpp"

#include <Frigga/Gui/GuiLayer.hpp>

#include <vulkan/vulkan.hpp>

#include "Freya/Core/Device.hpp"
#include "Freya/Core/IBLService.hpp"

namespace editor
{
    namespace
    {
        GraphicsReloadResult FinishReload(const skr::Arc<fra::Renderer> &renderer)
        {
            renderer->RebuildSwapChain();
            fg::GuiLayer::RecreateMainPipeline(renderer);
            return GraphicsReloadResult {.rebuiltSwapChain = true};
        }
    } // namespace

    GraphicsReloadResult ApplyShaderRoot(const skr::Arc<fra::Renderer> &renderer,
                                         const skr::Arc<fra::FreyaOptions> &options,
                                         std::string shaderRoot)
    {
        if(!renderer || !options)
        {
            return {};
        }

        options->shaderRoot = std::move(shaderRoot);
        return FinishReload(renderer);
    }

    GraphicsReloadResult ApplyEnvironmentMap(const skr::Arc<skr::ServiceProvider> &services,
                                             const skr::Arc<fra::Renderer> &renderer,
                                             const skr::Arc<fra::FreyaOptions> &options,
                                             std::string environmentMapPath)
    {
        GraphicsReloadResult result {};
        if(!services || !renderer || !options)
        {
            return result;
        }

        options->environmentMapPath = std::move(environmentMapPath);

        if(services->Remove<fra::IBLService>())
        {
            services->AddSingleton<fra::IBLService>(
                [services](skr::ServiceProvider &provider) {
                    return skr::MakeArc<fra::IBLService>(
                        provider.GetService<fra::Device>(), services,
                        provider.GetService<fra::FreyaOptions>());
                });
            result.rebuiltIbl = true;
        }

        result.rebuiltSwapChain = FinishReload(renderer).rebuiltSwapChain;
        return result;
    }

    GraphicsReloadResult ApplyReverseZ(const skr::Arc<fra::Renderer> &renderer,
                                       const skr::Arc<fra::FreyaOptions> &options,
                                       const bool reverseZ)
    {
        if(!renderer || !options)
        {
            return {};
        }

        options->ReverseZ = reverseZ;
        return FinishReload(renderer);
    }

} // namespace editor
