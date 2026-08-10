#include "GameplayPluginHost.hpp"

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <dlfcn.h>
#endif

namespace FRIGGA_NAMESPACE
{
    namespace
    {
#ifdef _WIN32
        void *OpenLibrary(const char *path)
        {
            return static_cast<void *>(LoadLibraryA(path));
        }

        void CloseLibrary(void *handle)
        {
            if(handle)
            {
                FreeLibrary(static_cast<HMODULE>(handle));
            }
        }

        void *LookupSymbol(void *handle, const char *name)
        {
            return reinterpret_cast<void *>(
                GetProcAddress(static_cast<HMODULE>(handle), name));
        }

        std::string LastDlError()
        {
            return "LoadLibrary/GetProcAddress failed";
        }
#else
        void *OpenLibrary(const char *path)
        {
        return dlopen(path, RTLD_NOW | RTLD_GLOBAL);
        }

        void CloseLibrary(void *handle)
        {
            if(handle)
            {
                dlclose(handle);
            }
        }

        void *LookupSymbol(void *handle, const char *name)
        {
            return dlsym(handle, name);
        }

        std::string LastDlError()
        {
            const char *err = dlerror();
            return err ? err : "dlopen/dlsym failed";
        }
#endif
    } // namespace

    GameplayPluginHost::GameplayPluginHost(
        const skr::Arc<fr::Registry> &registry,
        const skr::Arc<UserComponentRegistry> &userComponents,
        const skr::Arc<skr::Logger<GameplayPluginHost>> &logger)
        : mRegistry(registry), mUserComponents(userComponents), mLogger(logger)
    {
    }

    GameplayPluginHost::~GameplayPluginHost()
    {
        Unload();
    }

    bool GameplayPluginHost::IsLoaded() const
    {
        std::lock_guard lock(mMutex);
        return mPlugin != nullptr && mApi != nullptr;
    }

    std::vector<std::string> GameplayPluginHost::GetRegisteredTypeIds() const
    {
        std::vector<std::string> ids;
        if(!mUserComponents)
        {
            return ids;
        }
        for(const auto &ops : mUserComponents->GetTypes())
        {
            ids.push_back(ops.typeId);
        }
        return ids;
    }

    bool GameplayPluginHost::Load(const std::filesystem::path &libraryPath)
    {
        std::lock_guard lock(mMutex);
        unloadUnlocked();
        return loadUnlocked(libraryPath);
    }

    bool GameplayPluginHost::Reload()
    {
        std::lock_guard lock(mMutex);
        if(mLibraryPath.empty())
        {
            mLastError = "No plugin library path set";
            return false;
        }
        const auto path = mLibraryPath;
        unloadUnlocked();
        return loadUnlocked(path);
    }

    void GameplayPluginHost::Unload()
    {
        std::lock_guard lock(mMutex);
        unloadUnlocked();
    }

    void GameplayPluginHost::UpdatePlugin(float deltaTime)
    {
        std::lock_guard lock(mMutex);
        if(mPlugin && mApi && mApi->on_update && mAttached)
        {
            mApi->on_update(mPlugin, deltaTime);
        }
    }

    bool GameplayPluginHost::loadUnlocked(const std::filesystem::path &libraryPath)
    {
        mLastError.clear();

        if(libraryPath.empty() || !std::filesystem::exists(libraryPath))
        {
            mLastError = "Plugin library not found: " + libraryPath.string();
            mLogger->LogWarning("{}", mLastError);
            return false;
        }

        const auto absolute = std::filesystem::weakly_canonical(libraryPath);
        mHandle             = OpenLibrary(absolute.string().c_str());
        if(!mHandle)
        {
            mLastError = LastDlError();
            mLogger->LogError("Failed to load plugin {}: {}", absolute.string(), mLastError);
            return false;
        }

        using ApiFn = const FriPluginApi *(*)();
        auto *symbol = LookupSymbol(mHandle, "fri_plugin_api");
        if(!symbol)
        {
            mLastError = "Missing export fri_plugin_api: " + LastDlError();
            mLogger->LogError("{}", mLastError);
            CloseLibrary(mHandle);
            mHandle = nullptr;
            return false;
        }

        auto *getApi = reinterpret_cast<ApiFn>(symbol);
        mApi         = getApi();
        if(!mApi || !mApi->create || !mApi->destroy)
        {
            mLastError = "Invalid FriPluginApi table";
            mLogger->LogError("{}", mLastError);
            CloseLibrary(mHandle);
            mHandle = nullptr;
            mApi    = nullptr;
            return false;
        }

        mPlugin = mApi->create();
        if(!mPlugin)
        {
            mLastError = "fri_plugin_api create() returned null";
            mLogger->LogError("{}", mLastError);
            CloseLibrary(mHandle);
            mHandle = nullptr;
            mApi    = nullptr;
            return false;
        }

        mLibraryPath = absolute;
        attachUnlocked();
        mLogger->LogInformation("Loaded gameplay plugin {}", absolute.string());
        return true;
    }

    void GameplayPluginHost::unloadUnlocked()
    {
        detachUnlocked();
        if(mPlugin && mApi && mApi->destroy)
        {
            mApi->destroy(mPlugin);
        }
        mPlugin = nullptr;
        mApi    = nullptr;
        if(mHandle)
        {
            CloseLibrary(mHandle);
            mHandle = nullptr;
        }
    }

    void GameplayPluginHost::attachUnlocked()
    {
        if(!mPlugin || !mApi || !mApi->on_attach || mAttached)
        {
            return;
        }

        FriHost host {.registry         = mRegistry.get(),
                      .user_components  = mUserComponents.get()};
        mApi->on_attach(mPlugin, &host);
        mAttached = true;

        const auto types = mUserComponents->GetTypes();
        std::string registered;
        for(const auto &ops : types)
        {
            if(!registered.empty())
            {
                registered += ", ";
            }
            registered += ops.typeId;
        }
        mLogger->LogInformation("Gameplay plugin registered {} user component(s): {}",
                                types.size(),
                                registered.empty() ? "(none)" : registered);
    }

    void GameplayPluginHost::detachUnlocked()
    {
        if(!mPlugin || !mApi || !mAttached)
        {
            mAttached = false;
            return;
        }

        // Strip Freyr SoA instances + unregister while .so (and ops) are still mapped.
        mUserComponents->DetachAll(*mRegistry);

        if(mApi->on_detach)
        {
            mApi->on_detach(mPlugin);
        }
        mAttached = false;
    }

} // namespace FRIGGA_NAMESPACE
