#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    /// On-demand, thread-safe cache of prefab JSON keyed by normalized resource path.
    /// Does not preload; entries are created on first GetOrLoad.
    class PrefabCache
    {
      public:
        static PrefabCache &Instance();

        /// Load from disk on miss. Returns nullptr if the file cannot be read.
        [[nodiscard]] std::shared_ptr<const std::string> GetOrLoad(
            const std::filesystem::path &path);

        void Invalidate(std::string_view cacheKey);
        void InvalidatePath(const std::filesystem::path &path);
        void Clear();

        [[nodiscard]] bool Contains(std::string_view cacheKey) const;
        [[nodiscard]] std::size_t Size() const;
        /// Number of successful disk reads (test/diagnostic).
        [[nodiscard]] std::uint64_t DiskLoadCount() const;

        [[nodiscard]] static std::string MakeCacheKey(const std::filesystem::path &path);

      private:
        PrefabCache() = default;

        mutable std::mutex mMutex;
        std::unordered_map<std::string, std::shared_ptr<const std::string>> mEntries;
        std::uint64_t mDiskLoadCount = 0;
    };

} // namespace FRIGGA_NAMESPACE
