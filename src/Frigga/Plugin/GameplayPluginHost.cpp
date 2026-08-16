#include "Frigga/Plugin/GameplayPluginHost.hpp"

#include <Freyr/Core/SystemManager.hpp>

#include <chrono>
#include <system_error>

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
            return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(handle), name));
        }

        std::string LastDlError()
        {
            return "LoadLibrary/GetProcAddress failed";
        }
#else
        void *OpenLibrary(const char *path)
        {
            return dlopen(path, RTLD_NOW | RTLD_LOCAL);
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

        [[nodiscard]] std::filesystem::path
        MakeStagedLibraryPath(const std::filesystem::path &source, std::uint64_t generation)
        {
            const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
            auto staged = source;
            staged += ".reload-" + std::to_string(generation) + "-" + std::to_string(stamp);
            return staged;
        }

        void RemoveFileQuietly(const std::filesystem::path &path)
        {
            if(path.empty())
            {
                return;
            }
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        void CleanupStaleStagedCopies(const std::filesystem::path &source,
                                      const std::filesystem::path &keep)
        {
            const auto parent = source.parent_path();
            const auto prefix = source.filename().string() + ".reload-";
            std::error_code ec;
            if(!std::filesystem::exists(parent, ec))
            {
                return;
            }
            for(const auto &entry : std::filesystem::directory_iterator(parent, ec))
            {
                if(ec || !entry.is_regular_file(ec))
                {
                    continue;
                }
                const auto name = entry.path().filename().string();
                if(!name.starts_with(prefix))
                {
                    continue;
                }
                if(!keep.empty() && entry.path() == keep)
                {
                    continue;
                }
                RemoveFileQuietly(entry.path());
            }
        }
    } // namespace

    GameplayPluginHost::GameplayPluginHost(const skr::Arc<fr::Registry> &registry,
                                           const skr::Arc<UserComponentRegistry> &userComponents,
                                           const skr::Arc<fr::SystemManager> &systemManager,
                                           const skr::Arc<skr::ServiceProvider> &services,
                                           const skr::Arc<skr::Logger<GameplayPluginHost>> &logger)
        : mRegistry(registry), mUserComponents(userComponents), mSystemManager(systemManager),
          mServices(services), mLogger(logger)
    {
        FriKeepComponentInspectorSymbols();
    }

    GameplayPluginHost::~GameplayPluginHost()
    {
        Unload();
    }

    bool GameplayPluginHost::IsLoaded() const
    {
        std::lock_guard lock(mMutex);
        for(const auto &slot : mPlugins)
        {
            if(slot.plugin && slot.api && slot.attached)
            {
                return true;
            }
        }
        return false;
    }

    bool GameplayPluginHost::IsPluginLoaded(std::string_view id) const
    {
        std::lock_guard lock(mMutex);
        for(const auto &slot : mPlugins)
        {
            if(slot.id == id && slot.plugin && slot.attached)
            {
                return true;
            }
        }
        return false;
    }

    std::size_t GameplayPluginHost::LoadedCount() const
    {
        std::lock_guard lock(mMutex);
        std::size_t count = 0;
        for(const auto &slot : mPlugins)
        {
            if(slot.plugin && slot.attached)
            {
                ++count;
            }
        }
        return count;
    }

    std::vector<std::string> GameplayPluginHost::GetLoadedPluginIds() const
    {
        std::lock_guard lock(mMutex);
        std::vector<std::string> ids;
        for(const auto &slot : mPlugins)
        {
            if(slot.plugin && slot.attached)
            {
                ids.push_back(slot.id);
            }
        }
        return ids;
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

    bool GameplayPluginHost::LoadAll(const std::vector<PluginLoadRequest> &plugins)
    {
        std::lock_guard lock(mMutex);
        unloadAllUnlocked(/*preserveUserComponents=*/true);
        mLastError.clear();
        bool any = false;
        for(std::size_t i = 0; i < plugins.size(); ++i)
        {
            const bool last = i + 1 == plugins.size();
            if(loadUnlocked(plugins[i], last))
            {
                any = true;
            }
        }
        if(!plugins.empty() && !any)
        {
            if(mLastError.empty())
            {
                mLastError = "No plugin libraries could be loaded";
            }
            return false;
        }
        return true;
    }

    bool GameplayPluginHost::Load(const std::filesystem::path &libraryPath)
    {
        return LoadAll({PluginLoadRequest {.id = "gameplay", .libraryPath = libraryPath}});
    }

    bool GameplayPluginHost::Reload()
    {
        std::vector<PluginLoadRequest> requests;
        {
            std::lock_guard lock(mMutex);
            if(mPlugins.empty())
            {
                mLastError = "No plugin library path set";
                return false;
            }
            for(const auto &slot : mPlugins)
            {
                requests.push_back(PluginLoadRequest {
                    .id = slot.id, .name = slot.name, .libraryPath = slot.libraryPath});
            }
        }
        return LoadAll(requests);
    }

    void GameplayPluginHost::Unload()
    {
        std::lock_guard lock(mMutex);
        unloadAllUnlocked(/*preserveUserComponents=*/false);
    }

    void GameplayPluginHost::UpdatePlugin(float deltaTime)
    {
        std::lock_guard lock(mMutex);
        for(auto &slot : mPlugins)
        {
            if(slot.plugin && slot.api && slot.api->on_update && slot.attached)
            {
                slot.api->on_update(slot.plugin, deltaTime);
            }
        }
    }

    bool GameplayPluginHost::loadUnlocked(const PluginLoadRequest &request, bool restoreAfterAttach)
    {
        const auto &libraryPath = request.libraryPath;
        if(libraryPath.empty() || !std::filesystem::exists(libraryPath))
        {
            mLastError = "Plugin library not found: " + libraryPath.string();
            mLogger->LogWarning("{}", mLastError);
            return false;
        }

        const auto absolute = std::filesystem::weakly_canonical(libraryPath);
        const auto staged   = MakeStagedLibraryPath(absolute, ++mLoadGeneration);
        {
            std::error_code ec;
            std::filesystem::copy_file(absolute, staged,
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if(ec)
            {
                mLastError = "Failed to stage plugin for reload: " + ec.message();
                mLogger->LogError("{} ({})", mLastError, staged.string());
                return false;
            }
        }

        LoadedPlugin slot;
        slot.id          = request.id.empty() ? absolute.stem().string() : request.id;
        slot.name        = request.name.empty() ? slot.id : request.name;
        slot.libraryPath = absolute;
        slot.handle      = OpenLibrary(staged.string().c_str());
        if(!slot.handle)
        {
            mLastError = LastDlError();
            mLogger->LogError("Failed to load plugin {}: {}", staged.string(), mLastError);
            RemoveFileQuietly(staged);
            return false;
        }

        using ApiFn  = const FriPluginApi *(*)();
        auto *symbol = LookupSymbol(slot.handle, "fri_plugin_api");
        if(!symbol)
        {
            mLastError = "Missing export fri_plugin_api: " + LastDlError();
            mLogger->LogError("{}", mLastError);
            CloseLibrary(slot.handle);
            RemoveFileQuietly(staged);
            return false;
        }

        auto *getApi = reinterpret_cast<ApiFn>(symbol);
        slot.api     = getApi();
        if(!slot.api || !slot.api->create || !slot.api->destroy)
        {
            mLastError = "Invalid FriPluginApi table";
            mLogger->LogError("{}", mLastError);
            CloseLibrary(slot.handle);
            RemoveFileQuietly(staged);
            return false;
        }

        slot.plugin = slot.api->create();
        if(!slot.plugin)
        {
            mLastError = "fri_plugin_api create() returned null";
            mLogger->LogError("{}", mLastError);
            CloseLibrary(slot.handle);
            RemoveFileQuietly(staged);
            return false;
        }

        slot.stagedLibraryPath = staged;
        CleanupStaleStagedCopies(absolute, staged);
        attachUnlocked(slot, restoreAfterAttach);
        mLogger->LogInformation("Loaded plugin {} from {} (staged {})", slot.id, absolute.string(),
                                staged.filename().string());
        mPlugins.push_back(std::move(slot));
        return true;
    }

    void GameplayPluginHost::unloadAllUnlocked(bool preserveUserComponents)
    {
        if(preserveUserComponents && mPendingRestore.entries.empty() && mUserComponents)
        {
            mPendingRestore = mUserComponents->CaptureAll(*mRegistry);
            mLogger->LogInformation(
                "Captured {} gameplay component instance(s) before plugin unload",
                mPendingRestore.entries.size());
        }
        else if(!preserveUserComponents)
        {
            mPendingRestore = {};
        }

        if(mUserComponents)
        {
            mUserComponents->DetachAll(*mRegistry);
        }

        for(auto it = mPlugins.rbegin(); it != mPlugins.rend(); ++it)
        {
            if(it->plugin && it->api && it->attached && it->api->on_detach)
            {
                it->api->on_detach(it->plugin);
            }
            it->attached = false;
            unloadOneUnlocked(*it, preserveUserComponents);
        }
        mPlugins.clear();
    }

    void GameplayPluginHost::unloadOneUnlocked(LoadedPlugin &slot, bool)
    {
        if(slot.plugin && slot.api && slot.api->destroy)
        {
            slot.api->destroy(slot.plugin);
        }
        slot.plugin = nullptr;
        slot.api    = nullptr;
        if(slot.handle)
        {
            CloseLibrary(slot.handle);
            slot.handle = nullptr;
        }
        RemoveFileQuietly(slot.stagedLibraryPath);
        slot.stagedLibraryPath.clear();
    }

    void GameplayPluginHost::attachUnlocked(LoadedPlugin &slot, bool restoreAfterAttach)
    {
        if(!slot.plugin || !slot.api || !slot.api->on_attach || slot.attached)
        {
            return;
        }

        FriHost host {.registry        = mRegistry.get(),
                      .user_components = mUserComponents.get(),
                      .system_manager  = mSystemManager.get(),
                      .services        = mServices.get(),
                      .plugin_id       = slot.id.c_str(),
                      .plugin_name     = slot.name.c_str()};
        slot.api->on_attach(slot.plugin, &host);
        slot.attached = true;

        mLogger->LogInformation("Plugin {} attached; {} user component type(s) total", slot.id,
                                mUserComponents->GetTypes().size());

        if(const auto deferred = mUserComponents->ApplyDeferred(*mRegistry); deferred > 0)
        {
            mLogger->LogInformation(
                "Applied {} deferred gameplay component instance(s) after plugin load", deferred);
        }

        if(restoreAfterAttach && !mPendingRestore.entries.empty())
        {
            const auto restored = mUserComponents->RestoreAll(*mRegistry, mPendingRestore);
            mLogger->LogInformation(
                "Restored {} gameplay component instance(s) after plugin load", restored);
            mPendingRestore = {};
        }
    }

} // namespace FRIGGA_NAMESPACE
