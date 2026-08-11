#pragma once

/**
 * @file FriPluginModule.hpp
 * @brief Fluent registration helpers for Frigga gameplay plugins.
 *
 * Define the plugin with FRI_PLUGIN_MODULE:
 *
 * @code
 * FRI_PLUGIN_MODULE(plugin)
 * {
 *     plugin.Component<Health>()
 *           .System<GameplaySystem>()
 *           .Singleton<MyConfig>();
 * }
 * @endcode
 */

#include "frigga_plugin.h"

#include "Frigga/ECS/UserComponentReflection.hpp"

#include <Freyr/Core/SystemManager.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace fri_plugin_detail
    {
        [[nodiscard]] inline std::string ShortTypeName(std::string_view full)
        {
            const auto pos = full.rfind(':');
            if(pos == std::string_view::npos || pos + 1 >= full.size())
            {
                return std::string(full);
            }
            return std::string(full.substr(pos + 1));
        }
    } // namespace fri_plugin_detail

    struct FriPluginRuntime
    {
        fr::SystemManager *systemManager = nullptr;
        skr::ServiceProvider *services   = nullptr;
        std::vector<std::function<void()>> detachOps;

        void Detach()
        {
            for(auto it = detachOps.rbegin(); it != detachOps.rend(); ++it)
            {
                (*it)();
            }
            detachOps.clear();
            systemManager = nullptr;
            services      = nullptr;
        }
    };
} // namespace FRIGGA_NAMESPACE

struct FriPlugin
{
    fg::FriPluginRuntime runtime;
};

namespace FRIGGA_NAMESPACE
{
    class FriPluginBuilder
    {
      public:
        FriPluginBuilder(FriPlugin &plugin, const FriHost &host)
            : mPlugin(&plugin),
              mRegistry(static_cast<fr::Registry *>(host.registry)),
              mUserComponents(static_cast<UserComponentRegistry *>(host.user_components)),
              mSystemManager(static_cast<fr::SystemManager *>(host.system_manager)),
              mServices(static_cast<skr::ServiceProvider *>(host.services))
        {
        }

        [[nodiscard]] bool IsValid() const
        {
            return mRegistry && mUserComponents && mSystemManager && mServices;
        }

        template <typename T>
            requires fr::IsComponent<T>
        FriPluginBuilder &Component(std::string_view typeId = {},
                                    std::string_view displayName = {})
        {
            if(!IsValid())
            {
                return *this;
            }

            const std::string id =
                typeId.empty() ? fri_plugin_detail::ShortTypeName(refl::type_name<T>())
                               : std::string(typeId);
            FriRegisterUserComponent<T>(*mRegistry, *mUserComponents, id, displayName);
            return *this;
        }

        /// Registers T as a Freyr system (host Singleton + pipeline). Default: Simulation.
        template <typename T>
            requires fr::IsSystem<T>
        FriPluginBuilder &System(std::string_view pipeline = "Simulation")
        {
            if(!IsValid())
            {
                return *this;
            }

            mServices->AddSingleton<T>();
            const auto pipelineId = mSystemManager->FindPipelineId(pipeline);
            if(!pipelineId)
            {
                (void)mServices->Remove<T>();
                return *this;
            }

            mSystemManager->RegisterSystem<T>(*pipelineId);
            mPlugin->runtime.systemManager = mSystemManager;
            mPlugin->runtime.services      = mServices;
            mPlugin->runtime.detachOps.push_back([sm = mSystemManager, svc = mServices]() {
                (void)sm->UnregisterSystem<T>();
                (void)svc->Remove<T>();
            });
            return *this;
        }

        template <typename T>
        FriPluginBuilder &Singleton()
        {
            return addService<T>([](skr::ServiceProvider &svc) { svc.AddSingleton<T>(); });
        }

        template <typename T>
        FriPluginBuilder &Scoped()
        {
            return addService<T>([](skr::ServiceProvider &svc) { svc.AddScoped<T>(); });
        }

        template <typename T>
        FriPluginBuilder &Transient()
        {
            return addService<T>([](skr::ServiceProvider &svc) { svc.AddTransient<T>(); });
        }

      private:
        template <typename T, typename AddFn>
        FriPluginBuilder &addService(AddFn &&add)
        {
            if(!IsValid())
            {
                return *this;
            }

            add(*mServices);
            mPlugin->runtime.services = mServices;
            mPlugin->runtime.detachOps.push_back([svc = mServices]() { (void)svc->Remove<T>(); });
            return *this;
        }

        FriPlugin *mPlugin                     = nullptr;
        fr::Registry *mRegistry                = nullptr;
        UserComponentRegistry *mUserComponents = nullptr;
        fr::SystemManager *mSystemManager      = nullptr;
        skr::ServiceProvider *mServices        = nullptr;
    };
} // namespace FRIGGA_NAMESPACE

/**
 * Declares the plugin export and opens the fluent configure block.
 * @param name Identifier for the FriPluginBuilder parameter.
 */
#define FRI_PLUGIN_MODULE(name)                                                                    \
    static void fri_plugin_configure(fg::FriPluginBuilder &name);                                  \
                                                                                                   \
    static FriPlugin *fri_plugin_create(void)                                                      \
    {                                                                                              \
        return new FriPlugin();                                                                    \
    }                                                                                              \
                                                                                                   \
    static void fri_plugin_destroy(FriPlugin *plugin)                                              \
    {                                                                                              \
        if(plugin)                                                                                 \
        {                                                                                          \
            plugin->runtime.Detach();                                                              \
            delete plugin;                                                                         \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void fri_plugin_on_attach(FriPlugin *plugin, const FriHost *host)                       \
    {                                                                                              \
        if(!plugin || !host)                                                                       \
        {                                                                                          \
            return;                                                                                \
        }                                                                                          \
        plugin->runtime.Detach();                                                                  \
        fg::FriPluginBuilder builder(*plugin, *host);                                              \
        if(!builder.IsValid())                                                                     \
        {                                                                                          \
            return;                                                                                \
        }                                                                                          \
        fri_plugin_configure(builder);                                                             \
    }                                                                                              \
                                                                                                   \
    static void fri_plugin_on_detach(FriPlugin *plugin)                                            \
    {                                                                                              \
        if(plugin)                                                                                 \
        {                                                                                          \
            plugin->runtime.Detach();                                                              \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void fri_plugin_on_update(FriPlugin *, float)                                           \
    {                                                                                              \
    }                                                                                              \
                                                                                                   \
    extern "C" FRI_PLUGIN_API const FriPluginApi *fri_plugin_api(void)                             \
    {                                                                                              \
        static const FriPluginApi api {                                                            \
            .create    = fri_plugin_create,                                                        \
            .destroy   = fri_plugin_destroy,                                                       \
            .on_attach = fri_plugin_on_attach,                                                     \
            .on_detach = fri_plugin_on_detach,                                                     \
            .on_update = fri_plugin_on_update,                                                     \
        };                                                                                         \
        return &api;                                                                               \
    }                                                                                              \
                                                                                                   \
    static void fri_plugin_configure(fg::FriPluginBuilder &name)
