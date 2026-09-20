#include <Frigga/Scene/PrefabCache.hpp>

#include "Frigga/Asset/AssetRegistry.hpp"

#include <fstream>
#include <iterator>
#include <utility>

namespace FRIGGA_NAMESPACE
{
    PrefabCache &PrefabCache::Instance()
    {
        static PrefabCache cache;
        return cache;
    }

    std::string PrefabCache::MakeCacheKey(const std::filesystem::path &path)
    {
        auto relative = AssetRegistry::MakeRelativeToResources(path);
        if(relative.empty())
        {
            return path.lexically_normal().generic_string();
        }
        return relative.generic_string();
    }

    std::shared_ptr<const std::string> PrefabCache::GetOrLoad(const std::filesystem::path &path)
    {
        const auto key = MakeCacheKey(path);

        {
            std::lock_guard lock(mMutex);
            if(const auto it = mEntries.find(key); it != mEntries.end())
            {
                return it->second;
            }
        }

        std::ifstream file(path, std::ios::binary);
        if(!file)
        {
            return nullptr;
        }

        auto loaded = std::make_shared<std::string>(std::istreambuf_iterator<char>(file),
                                                    std::istreambuf_iterator<char>());
        if(!file.good() && !file.eof())
        {
            return nullptr;
        }

        std::shared_ptr<const std::string> stored = std::move(loaded);

        std::lock_guard lock(mMutex);
        if(const auto it = mEntries.find(key); it != mEntries.end())
        {
            return it->second;
        }

        mEntries.emplace(key, stored);
        ++mDiskLoadCount;
        return stored;
    }

    void PrefabCache::Invalidate(std::string_view cacheKey)
    {
        std::lock_guard lock(mMutex);
        mEntries.erase(std::string(cacheKey));
    }

    void PrefabCache::InvalidatePath(const std::filesystem::path &path)
    {
        Invalidate(MakeCacheKey(path));
    }

    void PrefabCache::Clear()
    {
        std::lock_guard lock(mMutex);
        mEntries.clear();
    }

    bool PrefabCache::Contains(std::string_view cacheKey) const
    {
        std::lock_guard lock(mMutex);
        return mEntries.contains(std::string(cacheKey));
    }

    std::size_t PrefabCache::Size() const
    {
        std::lock_guard lock(mMutex);
        return mEntries.size();
    }

    std::uint64_t PrefabCache::DiskLoadCount() const
    {
        std::lock_guard lock(mMutex);
        return mDiskLoadCount;
    }
} // namespace FRIGGA_NAMESPACE
