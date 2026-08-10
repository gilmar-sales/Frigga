#include "GameplayPluginHost.hpp"

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
            // RTLD_LOCAL: keep plugin symbols out of the global namespace so subsequent
            // plugins (and Freyr unique symbols) do not pin one another forever.
            // Unique path staging (below) is still required because STB_GNU_UNIQUE
            // symbols inside a single .so routinely prevent true dlclose unload.
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
            const auto stamp =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
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

        /// Best-effort GC of previous staged copies that failed to delete while unique-pinned.
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
        unloadUnlocked(/*preserveUserComponents=*/true);
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
        unloadUnlocked(/*preserveUserComponents=*/true);
        return loadUnlocked(path);
    }

    void GameplayPluginHost::Unload()
    {
        std::lock_guard lock(mMutex);
        unloadUnlocked(/*preserveUserComponents=*/false);
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

        // Always dlopen a unique copy. Linux often keeps the previous DSO mapped
        // (STB_GNU_UNIQUE Freyr component ids), so reopening the same path keeps
        // running stale OnAttach / systems after a rebuild.
        const auto staged = MakeStagedLibraryPath(absolute, ++mLoadGeneration);
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

        mHandle = OpenLibrary(staged.string().c_str());
        if(!mHandle)
        {
            mLastError = LastDlError();
            mLogger->LogError("Failed to load plugin {}: {}", staged.string(), mLastError);
            RemoveFileQuietly(staged);
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
            RemoveFileQuietly(staged);
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
            RemoveFileQuietly(staged);
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
            RemoveFileQuietly(staged);
            return false;
        }

        mLibraryPath       = absolute;
        mStagedLibraryPath = staged;
        CleanupStaleStagedCopies(absolute, staged);
        attachUnlocked();
        mLogger->LogInformation("Loaded gameplay plugin {} (staged {})", absolute.string(),
                                staged.filename().string());
        return true;
    }

    void GameplayPluginHost::unloadUnlocked(bool preserveUserComponents)
    {
        detachUnlocked(preserveUserComponents);
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

        // Best-effort: often fails while STB_GNU_UNIQUE keeps the inode mapped.
        RemoveFileQuietly(mStagedLibraryPath);
        mStagedLibraryPath.clear();

        if(!preserveUserComponents)
        {
            mPendingRestore = {};
        }
    }

    void GameplayPluginHost::attachUnlocked()
    {
        if(!mPlugin || !mApi || !mApi->on_attach || mAttached)
        {
            return;
        }

        FriHost host {.registry        = mRegistry.get(),
                      .user_components = mUserComponents.get()};
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

        if(!mPendingRestore.entries.empty())
        {
            const auto restored = mUserComponents->RestoreAll(*mRegistry, mPendingRestore);
            mLogger->LogInformation(
                "Restored {} gameplay component instance(s) after plugin load", restored);
            mPendingRestore = {};
        }
    }

    void GameplayPluginHost::detachUnlocked(bool preserveUserComponents)
    {
        if(!mPlugin || !mApi || !mAttached)
        {
            mAttached = false;
            if(!preserveUserComponents)
            {
                mPendingRestore = {};
            }
            return;
        }

        if(preserveUserComponents)
        {
            mPendingRestore = mUserComponents->CaptureAll(*mRegistry);
            mLogger->LogInformation(
                "Captured {} gameplay component instance(s) before plugin unload",
                mPendingRestore.entries.size());
        }
        else
        {
            mPendingRestore = {};
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
