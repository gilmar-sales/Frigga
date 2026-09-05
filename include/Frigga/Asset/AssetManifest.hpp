#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    class AssetManifest
    {
      public:
        static constexpr int CurrentVersion = 2;
        static constexpr std::string_view FileName = ".frigga-assets.json";

        struct ImportRecord
        {
            std::string relativePath;
            std::string type;
            std::string guid;
            std::uint64_t sourceSize = 0;
            std::uint64_t sourceHash = 0;
            std::int64_t sourceTimestamp = 0;
            std::string settings;
            std::vector<std::string> dependencies;
        };

        struct ValidationResult
        {
            std::vector<std::string> missing;
            std::vector<std::string> orphaned;
            std::vector<std::string> changed;

            [[nodiscard]] bool IsValid() const
            {
                return missing.empty() && orphaned.empty();
            }
        };

        bool Load(const std::filesystem::path &resourcesRoot, std::string *error = nullptr);
        bool Save(const std::filesystem::path &resourcesRoot, std::string *error = nullptr) const;

        [[nodiscard]] std::string GetOrCreate(std::string_view relativePath,
                                              std::string_view type);
        [[nodiscard]] std::string RecordImport(const std::filesystem::path &relativePath,
                                               std::string_view type,
                                               const std::filesystem::path &sourcePath,
                                               std::string settings = {},
                                               std::vector<std::string> dependencies = {});
        [[nodiscard]] const ImportRecord *Find(std::string_view relativePath,
                                               std::string_view type) const;
        [[nodiscard]] ValidationResult Validate(
            const std::filesystem::path &resourcesRoot) const;
        [[nodiscard]] std::vector<ImportRecord> Records() const;
        [[nodiscard]] const std::filesystem::path &Root() const
        {
            return mRoot;
        }

        void Clear();

      private:
        std::filesystem::path mRoot;
        std::unordered_map<std::string, ImportRecord> mEntries;
    };
} // namespace FRIGGA_NAMESPACE
