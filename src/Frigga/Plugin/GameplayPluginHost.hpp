#pragma once

#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Plugin/frigga_plugin.h"

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <filesystem>
#include <mutex>
#include <string>

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

        /// Unload any previous plugin, then load @p libraryPath.
        bool Load(const std::filesystem::path &libraryPath);
        bool Reload();
        void Unload();

        /// Forwards to the plugin when loaded (no-op otherwise).
        void UpdatePlugin(float deltaTime);

      private:
        bool loadUnlocked(const std::filesystem::path &libraryPath);
        void unloadUnlocked();
        void attachUnlocked();
        void detachUnlocked();

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<UserComponentRegistry> mUserComponents;
        skr::Arc<skr::Logger<GameplayPluginHost>> mLogger;

        mutable std::mutex mMutex;
        std::filesystem::path mLibraryPath;
        void *mHandle              = nullptr;
        const FriPluginApi *mApi   = nullptr;
        FriPlugin *mPlugin         = nullptr;
        bool mAttached             = false;
        std::string mLastError;
    };

} // namespace FRIGGA_NAMESPACE
