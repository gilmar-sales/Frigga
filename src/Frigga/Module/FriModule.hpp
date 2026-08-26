#pragma once

/**
 * @file FriModule.hpp
 * @brief Fluent registration helpers for Frigga gameplay modules.
 *
 * Define the module with FRI_MODULE:
 *
 * @code
 * FRI_MODULE(module)
 * {
 *     module.Component<Health>()
 *           .Component<CharacterControllerComponent>(
 *               "CharacterControllerComponent", "Character Controller",
 *               DrawCharacterController)
 *           .System<GameplaySystem>()
 *           .Singleton<MyConfig>();
 * }
 * @endcode
 *
 * A draw callback is optional. Without one, the Editor falls back to reflection.
 */

#include "frigga_module.h"
#include "Frigga/Module/FriPluginSdk.hpp"
#include "Frigga/Module/FriComponentInspector.hpp"

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
    namespace fri_module_detail
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
    } // namespace fri_module_detail

    struct FriModuleRuntime
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

struct FriModule
{
    fg::FriModuleRuntime runtime;
};

namespace FRIGGA_NAMESPACE
{
    class FriModuleBuilder
    {
      public:
        FriModuleBuilder(FriModule &module, const FriHost &host)
            : mModule(&module),
              mRegistry(static_cast<fr::Registry *>(host.registry)),
              mUserComponents(static_cast<UserComponentRegistry *>(host.user_components)),
              mSystemManager(static_cast<fr::SystemManager *>(host.system_manager)),
              mServices(static_cast<skr::ServiceProvider *>(host.services)),
              mModuleId(host.module_id ? host.module_id : ""),
              mModuleName(host.module_name && host.module_name[0] != '\0'
                              ? host.module_name
                              : mModuleId)
        {
        }

        [[nodiscard]] bool IsValid() const
        {
            return mRegistry && mUserComponents && mSystemManager && mServices;
        }

        template <typename T>
            requires fr::IsComponent<T>
        FriModuleBuilder &Component(std::string_view typeId = {},
                                    std::string_view displayName = {},
                                    UserComponentDetachPolicy detach =
                                        UserComponentDetachPolicy::Unregister)
        {
            return Component<T>(typeId, displayName, nullptr, detach);
        }

        template <typename T>
            requires fr::IsComponent<T>
        FriModuleBuilder &Component(std::string_view typeId, std::string_view displayName,
                                    FriDrawComponent<T> draw,
                                    UserComponentDetachPolicy detach =
                                        UserComponentDetachPolicy::Unregister)
        {
            if(!IsValid())
            {
                return *this;
            }

            const std::string id =
                typeId.empty() ? fri_module_detail::ShortTypeName(refl::type_name<T>())
                               : std::string(typeId);
            FriRegisterUserComponent<T>(*mRegistry, *mUserComponents, id, displayName, detach,
                                        draw, mModuleId, mModuleName);
            return *this;
        }

        /// Registers T as a Freyr system (host Singleton + pipeline). Default: Simulation.
        template <typename T>
            requires fr::IsSystem<T>
        FriModuleBuilder &System(std::string_view pipeline = "Simulation")
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
            mModule->runtime.systemManager = mSystemManager;
            mModule->runtime.services      = mServices;
            mModule->runtime.detachOps.push_back([sm = mSystemManager, svc = mServices]() {
                (void)sm->UnregisterSystem<T>();
                (void)svc->Remove<T>();
            });
            return *this;
        }

        template <typename T>
        FriModuleBuilder &Singleton()
        {
            return addService<T>([](skr::ServiceProvider &svc) { svc.AddSingleton<T>(); });
        }

        template <typename T>
        FriModuleBuilder &Scoped()
        {
            return addService<T>([](skr::ServiceProvider &svc) { svc.AddScoped<T>(); });
        }

        template <typename T>
        FriModuleBuilder &Transient()
        {
            return addService<T>([](skr::ServiceProvider &svc) { svc.AddTransient<T>(); });
        }

      private:
        template <typename T, typename AddFn>
        FriModuleBuilder &addService(AddFn &&add)
        {
            if(!IsValid())
            {
                return *this;
            }

            add(*mServices);
            mModule->runtime.services = mServices;
            mModule->runtime.detachOps.push_back([svc = mServices]() { (void)svc->Remove<T>(); });
            return *this;
        }

        FriModule *mModule                     = nullptr;
        fr::Registry *mRegistry                = nullptr;
        UserComponentRegistry *mUserComponents = nullptr;
        fr::SystemManager *mSystemManager      = nullptr;
        skr::ServiceProvider *mServices        = nullptr;
        std::string mModuleId;
        std::string mModuleName;
    };
} // namespace FRIGGA_NAMESPACE

/**
 * Declares the module export and opens the fluent configure block.
 * @param name Identifier for the FriModuleBuilder parameter.
 */
#define FRI_MODULE(name)                                                                           \
    static void fri_module_configure(fg::FriModuleBuilder &name);                                   \
                                                                                                   \
    static FriModule *fri_module_create(void)                                                      \
    {                                                                                              \
        return new FriModule();                                                                    \
    }                                                                                              \
                                                                                                   \
    static void fri_module_destroy(FriModule *module)                                              \
    {                                                                                              \
        if(module)                                                                                 \
        {                                                                                          \
            module->runtime.Detach();                                                              \
            delete module;                                                                         \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void fri_module_on_attach(FriModule *module, const FriHost *host)                       \
    {                                                                                              \
        if(!module || !host)                                                                       \
        {                                                                                          \
            return;                                                                                \
        }                                                                                          \
        module->runtime.Detach();                                                                  \
        fg::FriModuleBuilder builder(*module, *host);                                              \
        if(!builder.IsValid())                                                                     \
        {                                                                                          \
            return;                                                                                \
        }                                                                                          \
        fri_module_configure(builder);                                                             \
    }                                                                                              \
                                                                                                   \
    static void fri_module_on_detach(FriModule *module)                                            \
    {                                                                                              \
        if(module)                                                                                 \
        {                                                                                          \
            module->runtime.Detach();                                                              \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void fri_module_on_update(FriModule *, float)                                           \
    {                                                                                              \
    }                                                                                              \
                                                                                                   \
    extern "C" FRI_MODULE_API const FriModuleApi *fri_module_api(void)                             \
    {                                                                                              \
        static const FriModuleApi api {                                                            \
            .create    = fri_module_create,                                                        \
            .destroy   = fri_module_destroy,                                                       \
            .on_attach = fri_module_on_attach,                                                     \
            .on_detach = fri_module_on_detach,                                                     \
            .on_update = fri_module_on_update,                                                     \
        };                                                                                         \
        return &api;                                                                               \
    }                                                                                              \
                                                                                                   \
    static void fri_module_configure(fg::FriModuleBuilder &name)
