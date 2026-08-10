#include "UserComponentRegistry.hpp"

namespace FRIGGA_NAMESPACE
{

    void UserComponentRegistry::Register(UserComponentTypeDesc desc)
    {
        if(desc.typeId.empty())
        {
            return;
        }
        if(desc.displayName.empty())
        {
            desc.displayName = desc.typeId;
        }
        if(desc.defaultInstance.typeId.empty())
        {
            desc.defaultInstance.typeId = desc.typeId;
        }
        if(!desc.makeDefault)
        {
            const auto fallback = desc.defaultInstance;
            desc.makeDefault    = [fallback]() { return fallback; };
        }

        const std::string typeId = desc.typeId;
        std::lock_guard lock(mMutex);
        const bool isNew = !mTypes.contains(typeId);
        mTypes[typeId]   = std::move(desc);
        if(isNew)
        {
            mOrder.push_back(typeId);
        }
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

    std::optional<UserComponentTypeDesc> UserComponentRegistry::Find(std::string_view typeId) const
    {
        std::lock_guard lock(mMutex);
        const auto it = mTypes.find(std::string(typeId));
        if(it == mTypes.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<UserComponentTypeDesc> UserComponentRegistry::GetTypes() const
    {
        std::lock_guard lock(mMutex);
        std::vector<UserComponentTypeDesc> result;
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
