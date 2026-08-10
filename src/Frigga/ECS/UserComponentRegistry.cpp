#include "UserComponentRegistry.hpp"

namespace FRIGGA_NAMESPACE
{

    void UserComponentRegistry::Register(RuntimeComponentOps ops)
    {
        if(ops.typeId.empty() || !ops.addDefault || !ops.remove || !ops.has ||
           !ops.toInstance || !ops.fromInstance || !ops.removeFromAllEntities ||
           !ops.unregisterFromFreyr)
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

        ClearPluginTypes();
    }

    void UserComponentRegistry::ClearPluginTypes()
    {
        std::lock_guard lock(mMutex);
        mTypes.clear();
        mOrder.clear();
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
