#pragma once

#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Plugin/frigga_plugin.h"

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

    struct PluginLoadRequest
    {
        std::string           id;
        std::string           name;
        std::filesystem::path libraryPath;
    };

    /**
     * Loads / unloads gameplay shared libraries exposing fri_plugin_api().
     * Safe to call Reload from the main thread; UpdatePlugin is called from the ECS bridge.
     */
    class GameplayPluginHost
    {
      public:
        GameplayPluginHost(const skr::Arc<fr::Registry> &registry,
                           const skr::Arc<UserComponentRegistry> &userComponents,
                           const skr::Arc<fr::SystemManager> &systemManager,
                           const skr::Arc<skr::ServiceProvider> &services,
                           const skr::Arc<skr::Logger<GameplayPluginHost>> &logger);
        ~GameplayPluginHost();

        GameplayPluginHost(const GameplayPluginHost &)            = delete;
        GameplayPluginHost &operator=(const GameplayPluginHost &) = delete;

        [[nodiscard]] bool IsLoaded() const;
        [[nodiscard]] bool IsPluginLoaded(std::string_view id) const;
        [[nodiscard]] std::size_t LoadedCount() const;
        [[nodiscard]] std::vector<std::string> GetLoadedPluginIds() const;
        [[nodiscard]] const std::string &GetLastError() const
        {
            return mLastError;
        }
        /// TypeIds registered by the last successful attach (e.g. "Health, Player").
        [[nodiscard]] std::vector<std::string> GetRegisteredTypeIds() const;

        /// Unload every plugin, then load @p plugins in order (callers should put gameplay last).
        bool LoadAll(const std::vector<PluginLoadRequest> &plugins);
        /// Compatibility: replace the set with a single library.
        bool Load(const std::filesystem::path &libraryPath);
        bool Reload();
        void Unload();

        /// Forwards to every loaded plugin (no-op otherwise).
        void UpdatePlugin(float deltaTime);

      private:
        struct LoadedPlugin
        {
            std::string           id;
            std::string           name;
            std::filesystem::path libraryPath;
            std::filesystem::path stagedLibraryPath;
            void                 *handle   = nullptr;
            const FriPluginApi   *api      = nullptr;
            FriPlugin            *plugin   = nullptr;
            bool                  attached = false;
        };

        bool loadUnlocked(const PluginLoadRequest &request, bool restoreAfterAttach);
        void unloadAllUnlocked(bool preserveUserComponents);
        void unloadOneUnlocked(LoadedPlugin &slot, bool preserveUserComponents);
        void attachUnlocked(LoadedPlugin &slot, bool restoreAfterAttach);

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<UserComponentRegistry> mUserComponents;
        skr::Arc<fr::SystemManager> mSystemManager;
        skr::Arc<skr::ServiceProvider> mServices;
        skr::Arc<skr::Logger<GameplayPluginHost>> mLogger;

        mutable std::mutex mMutex;
        std::vector<LoadedPlugin> mPlugins;
        std::uint64_t mLoadGeneration = 0;
        std::string mLastError;
        UserComponentWorldSnapshot mPendingRestore {};
    };

} // namespace FRIGGA_NAMESPACE
