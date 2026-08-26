#include "UserComponentRegistry.hpp"

namespace FRIGGA_NAMESPACE
{

    void UserComponentRegistry::Register(RuntimeComponentOps ops)
    {
        if(ops.typeId.empty() || !ops.addDefault || !ops.remove || !ops.has ||
           !ops.toInstance || !ops.fromInstance || !ops.forEachEntity ||
           !ops.removeFromAllEntities || !ops.unregisterFromFreyr)
        {
            return;
        }
        if(ops.displayName.empty())
        {
            ops.displayName = ops.typeId;
        }
        if(ops.defaultInstance.typeId.empty())
        {
            ops.defaultInstance.typeId = ops.typeId;
        }

        const std::string typeId = ops.typeId;
        std::lock_guard lock(mMutex);
        const bool isNew = !mTypes.contains(typeId);
        mTypes[typeId]   = std::move(ops);
        if(isNew)
        {
            mOrder.push_back(typeId);
        }
    }

    void UserComponentRegistry::DetachAll(fr::Registry &registry)
    {
        std::vector<RuntimeComponentOps> snapshot;
        {
            std::lock_guard lock(mMutex);
            snapshot.reserve(mOrder.size());
            for(const auto &id : mOrder)
            {
                const auto it = mTypes.find(id);
                if(it != mTypes.end())
                {
                    snapshot.push_back(it->second);
                }
            }
        }

        for(const auto &ops : snapshot)
        {
            if(ops.removeFromAllEntities)
            {
                ops.removeFromAllEntities(registry);
            }
        }
        registry.ExecuteTasks();

        for(const auto &ops : snapshot)
        {
            if(ops.unregisterFromFreyr)
            {
                (void)ops.unregisterFromFreyr(registry);
            }
        }

        ClearModuleTypes();
    }

    void UserComponentRegistry::ClearModuleTypes()
    {
        std::lock_guard lock(mMutex);
        mTypes.clear();
        mOrder.clear();
    }

    UserComponentWorldSnapshot UserComponentRegistry::CaptureAll(fr::Registry &registry) const
    {
        UserComponentWorldSnapshot snapshot;
        const auto types = GetTypes();
        for(const auto &ops : types)
        {
            if(!ops.forEachEntity || !ops.toInstance)
            {
                continue;
            }
            ops.forEachEntity(registry, [&](fr::Entity entity) {
                UserComponentInstance instance {};
                if(!ops.toInstance(registry, entity, instance))
                {
                    return;
                }
                snapshot.entries.push_back(
                    UserComponentSnapshotEntry {.entity = entity, .instance = std::move(instance)});
            });
        }
        return snapshot;
    }

    std::size_t UserComponentRegistry::RestoreAll(fr::Registry &registry,
                                                  const UserComponentWorldSnapshot &snapshot)
    {
        std::size_t restored = 0;
        for(const auto &entry : snapshot.entries)
        {
            const auto ops = Find(entry.instance.typeId);
            if(!ops || !ops->fromInstance)
            {
                continue;
            }
            ops->fromInstance(registry, entry.entity, entry.instance);
            ++restored;
        }
        if(restored > 0)
        {
            registry.ExecuteTasks();
        }
        return restored;
    }

    void UserComponentRegistry::EnqueueDeferred(fr::Entity entity, UserComponentInstance instance)
    {
        if(instance.typeId.empty())
        {
            return;
        }

        std::lock_guard lock(mMutex);
        for(auto &entry : mDeferred)
        {
            if(entry.entity == entity && entry.instance.typeId == instance.typeId)
            {
                entry.instance = std::move(instance);
                return;
            }
        }
        mDeferred.push_back(
            UserComponentSnapshotEntry {.entity = entity, .instance = std::move(instance)});
    }

    void UserComponentRegistry::ClearDeferred()
    {
        std::lock_guard lock(mMutex);
        mDeferred.clear();
    }

    std::size_t UserComponentRegistry::ApplyDeferred(fr::Registry &registry)
    {
        std::vector<UserComponentSnapshotEntry> pending;
        {
            std::lock_guard lock(mMutex);
            pending = mDeferred;
        }

        if(pending.empty())
        {
            return 0;
        }

        std::vector<UserComponentSnapshotEntry> remaining;
        remaining.reserve(pending.size());
        std::size_t applied = 0;

        for(auto &entry : pending)
        {
            const auto ops = Find(entry.instance.typeId);
            if(!ops || !ops->fromInstance)
            {
                remaining.push_back(std::move(entry));
                continue;
            }
            ops->fromInstance(registry, entry.entity, entry.instance);
            ++applied;
        }

        if(applied > 0)
        {
            registry.ExecuteTasks();
        }

        {
            std::lock_guard lock(mMutex);
            mDeferred = std::move(remaining);
        }
        return applied;
    }

    std::vector<UserComponentSnapshotEntry> UserComponentRegistry::GetDeferred() const
    {
        std::lock_guard lock(mMutex);
        return mDeferred;
    }

    std::vector<UserComponentSnapshotEntry> UserComponentRegistry::GetDeferredForEntity(
        fr::Entity entity) const
    {
        std::lock_guard lock(mMutex);
        std::vector<UserComponentSnapshotEntry> result;
        for(const auto &entry : mDeferred)
        {
            if(entry.entity == entity)
            {
                result.push_back(entry);
            }
        }
        return result;
    }

    bool UserComponentRegistry::Has(std::string_view typeId) const
    {
        std::lock_guard lock(mMutex);
        return mTypes.contains(std::string(typeId));
    }

    std::optional<RuntimeComponentOps> UserComponentRegistry::Find(std::string_view typeId) const
    {
        std::lock_guard lock(mMutex);
        const auto it = mTypes.find(std::string(typeId));
        if(it == mTypes.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<RuntimeComponentOps> UserComponentRegistry::GetTypes() const
    {
        std::lock_guard lock(mMutex);
        std::vector<RuntimeComponentOps> result;
        result.reserve(mOrder.size());
        for(const auto &id : mOrder)
        {
            const auto it = mTypes.find(id);
            if(it != mTypes.end())
            {
                result.push_back(it->second);
            }
        }
        return result;
    }

} // namespace FRIGGA_NAMESPACE
