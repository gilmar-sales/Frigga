#pragma once

#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Module/frigga_module.h"

#include <Freyr/Core/SystemManager.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct ModuleLoadRequest
    {
        std::string           id;
        std::string           name;
        std::filesystem::path libraryPath;
    };

    /**
     * Loads / unloads gameplay shared libraries exposing fri_module_api().
     * Safe to call Reload from the main thread; UpdateModule is called from the ECS bridge.
     */
    class GameplayModuleHost
    {
      public:
        GameplayModuleHost(const skr::Arc<fr::Registry> &registry,
                           const skr::Arc<UserComponentRegistry> &userComponents,
                           const skr::Arc<fr::SystemManager> &systemManager,
                           const skr::Arc<skr::ServiceProvider> &services,
                           const skr::Arc<skr::Logger<GameplayModuleHost>> &logger);
        ~GameplayModuleHost();

        GameplayModuleHost(const GameplayModuleHost &)            = delete;
        GameplayModuleHost &operator=(const GameplayModuleHost &) = delete;

        [[nodiscard]] bool IsLoaded() const;
        [[nodiscard]] bool IsModuleLoaded(std::string_view id) const;
        [[nodiscard]] std::size_t LoadedCount() const;
        [[nodiscard]] std::vector<std::string> GetLoadedModuleIds() const;
        [[nodiscard]] const std::string &GetLastError() const
        {
            return mLastError;
        }
        /// TypeIds registered by the last successful attach (e.g. "Health, Player").
        [[nodiscard]] std::vector<std::string> GetRegisteredTypeIds() const;

        /// Unload every module, then load @p modules in order (callers should put gameplay last).
        bool LoadAll(const std::vector<ModuleLoadRequest> &modules);
        /// Compatibility: replace the set with a single library.
        bool Load(const std::filesystem::path &libraryPath);
        bool Reload();
        void Unload();

        /// Forwards to every loaded module (no-op otherwise).
        void UpdateModule(float deltaTime);

      private:
        struct LoadedModule
        {
            std::string           id;
            std::string           name;
            std::filesystem::path libraryPath;
            std::filesystem::path stagedLibraryPath;
            void                 *handle   = nullptr;
            const FriModuleApi   *api      = nullptr;
            FriModule            *module   = nullptr;
            bool                  attached = false;
        };

        bool loadUnlocked(const ModuleLoadRequest &request, bool restoreAfterAttach);
        void unloadAllUnlocked(bool preserveUserComponents);
        void unloadOneUnlocked(LoadedModule &slot, bool preserveUserComponents);
        void attachUnlocked(LoadedModule &slot, bool restoreAfterAttach);

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<UserComponentRegistry> mUserComponents;
        skr::Arc<fr::SystemManager> mSystemManager;
        skr::Arc<skr::ServiceProvider> mServices;
        skr::Arc<skr::Logger<GameplayModuleHost>> mLogger;

        mutable std::mutex mMutex;
        std::vector<LoadedModule> mModules;
        std::uint64_t mLoadGeneration = 0;
        std::string mLastError;
        UserComponentWorldSnapshot mPendingRestore {};
    };

} // namespace FRIGGA_NAMESPACE
