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
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /**
     * Loads / unloads a gameplay shared library exposing fri_plugin_api().
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
        [[nodiscard]] const std::filesystem::path &GetLibraryPath() const
        {
            return mLibraryPath;
        }
        [[nodiscard]] const std::string &GetLastError() const
        {
            return mLastError;
        }
        /// TypeIds registered by the last successful attach (e.g. "Health, Player").
        [[nodiscard]] std::vector<std::string> GetRegisteredTypeIds() const;

        /// Unload any previous plugin, then load @p libraryPath.
        /// Preserves gameplay component instances across the swap when possible.
        bool Load(const std::filesystem::path &libraryPath);
        bool Reload();
        void Unload();

        /// Forwards to the plugin when loaded (no-op otherwise).
        void UpdatePlugin(float deltaTime);

      private:
        bool loadUnlocked(const std::filesystem::path &libraryPath);
        void unloadUnlocked(bool preserveUserComponents);
        void attachUnlocked();
        void detachUnlocked(bool preserveUserComponents);

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<UserComponentRegistry> mUserComponents;
        skr::Arc<fr::SystemManager> mSystemManager;
        skr::Arc<skr::ServiceProvider> mServices;
        skr::Arc<skr::Logger<GameplayPluginHost>> mLogger;

        mutable std::mutex mMutex;
        std::filesystem::path mLibraryPath;
        /// Unique copy actually passed to dlopen (avoids stale-inode hot-reload).
        std::filesystem::path mStagedLibraryPath;
        void *mHandle            = nullptr;
        const FriPluginApi *mApi = nullptr;
        FriPlugin *mPlugin       = nullptr;
        bool mAttached           = false;
        std::uint64_t mLoadGeneration = 0;
        std::string mLastError;
        UserComponentWorldSnapshot mPendingRestore {};
    };

} // namespace FRIGGA_NAMESPACE
