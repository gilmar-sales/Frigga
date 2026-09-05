#include <Frigga/Asset/AssetManifest.hpp>
#include <Frigga/Serialization/FormatVersions.hpp>

#include <simdjson.h>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <sstream>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        std::string EscapeJson(std::string_view value)
        {
            std::string out;
            out.reserve(value.size() + 8);
            for(const char ch : value)
            {
                if(ch == '"' || ch == '\\')
                {
                    out.push_back('\\');
                }
                out.push_back(ch);
            }
            return out;
        }

        std::string StableGuid(std::string_view type, std::string_view path)
        {
            // FNV-1a provides a deterministic seed for new entries. The manifest
            // makes the resulting identity persistent for the project.
            std::uint64_t hash = 14695981039346656037ull;
            for(const char ch : std::string(type) + ":" + std::string(path))
            {
                hash ^= static_cast<unsigned char>(ch);
                hash *= 1099511628211ull;
            }
            return std::format("asset-{0:016x}", hash);
        }

        std::uint64_t HashFile(const std::filesystem::path &path)
        {
            std::ifstream file(path, std::ios::binary);
            if(!file)
            {
                return 0;
            }
            std::uint64_t hash = 14695981039346656037ull;
            std::array<char, 8192> buffer {};
            while(file)
            {
                file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                for(std::streamsize i = 0; i < file.gcount(); ++i)
                {
                    hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
                    hash *= 1099511628211ull;
                }
            }
            return hash;
        }

        std::int64_t FileTimestamp(const std::filesystem::path &path)
        {
            std::error_code ec;
            const auto timestamp = std::filesystem::last_write_time(path, ec);
            return ec ? 0 : timestamp.time_since_epoch().count();
        }

        std::string Key(std::string_view type, std::string_view path)
        {
            return std::string(type) + ":" + std::string(path);
        }

        std::string EscapeJson(std::string_view value);
    } // namespace

    bool AssetManifest::Load(const std::filesystem::path &resourcesRoot, std::string *error)
    {
        Clear();
        mRoot = resourcesRoot;
        const auto path = resourcesRoot / FileName;
        if(!std::filesystem::exists(path))
        {
            return true;
        }

        std::ifstream file(path, std::ios::binary);
        if(!file)
        {
            if(error)
            {
                *error = "Unable to read asset manifest: " + path.string();
            }
            return false;
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        simdjson::dom::parser parser;
        simdjson::dom::element document;
        if(const auto result = parser.parse(contents.str()).get(document);
           result != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "Invalid asset manifest JSON: " +
                         std::string(simdjson::error_message(result));
            }
            return false;
        }

        simdjson::dom::object root;
        if(document.get_object().get(root) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "Asset manifest root must be an object";
            }
            return false;
        }

        auto version = root.at_key("version");
        if(version.error() == simdjson::error_code::SUCCESS)
        {
            std::int64_t value = 0;
            if(version.get_int64().get(value) != simdjson::error_code::SUCCESS ||
               value > FormatVersion::AssetManifest)
            {
                if(error)
                {
                    *error = "Unsupported asset manifest version";
                }
                return false;
            }
        }

        auto assets = root.at_key("assets");
        if(assets.error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            return true;
        }
        simdjson::dom::array entries;
        if(assets.error() != simdjson::error_code::SUCCESS ||
           assets.get_array().get(entries) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "Asset manifest field 'assets' must be an array";
            }
            return false;
        }

        for(const auto element : entries)
        {
            simdjson::dom::object object;
            if(element.get_object().get(object) != simdjson::error_code::SUCCESS)
            {
                if(error)
                {
                    *error = "Asset manifest entries must be objects";
                }
                return false;
            }
            std::string_view relativePath;
            std::string_view type;
            std::string_view guid;
            auto pathField = object.at_key("path");
            auto typeField = object.at_key("type");
            auto guidField = object.at_key("guid");
            if(pathField.error() != simdjson::error_code::SUCCESS ||
               typeField.error() != simdjson::error_code::SUCCESS ||
               guidField.error() != simdjson::error_code::SUCCESS ||
               pathField.get_string().get(relativePath) != simdjson::error_code::SUCCESS ||
               typeField.get_string().get(type) != simdjson::error_code::SUCCESS ||
               guidField.get_string().get(guid) != simdjson::error_code::SUCCESS)
            {
                if(error)
                {
                    *error = "Asset manifest entry requires path, type and guid strings";
                }
                return false;
            }
            ImportRecord record {
                .relativePath = std::string(relativePath),
                .type         = std::string(type),
                .guid         = std::string(guid),
            };
            auto size = object.at_key("sourceSize");
            if(size.error() == simdjson::error_code::SUCCESS)
            {
                std::uint64_t value = 0;
                if(size.get_uint64().get(value) != simdjson::error_code::SUCCESS)
                {
                    if(error)
                    {
                        *error = "Asset manifest sourceSize must be an unsigned integer";
                    }
                    return false;
                }
                record.sourceSize = value;
            }
            auto hash = object.at_key("sourceHash");
            if(hash.error() == simdjson::error_code::SUCCESS)
            {
                std::uint64_t value = 0;
                if(hash.get_uint64().get(value) != simdjson::error_code::SUCCESS)
                {
                    if(error)
                    {
                        *error = "Asset manifest sourceHash must be an unsigned integer";
                    }
                    return false;
                }
                record.sourceHash = value;
            }
            auto timestamp = object.at_key("sourceTimestamp");
            if(timestamp.error() == simdjson::error_code::SUCCESS)
            {
                std::int64_t value = 0;
                if(timestamp.get_int64().get(value) != simdjson::error_code::SUCCESS)
                {
                    if(error)
                    {
                        *error = "Asset manifest sourceTimestamp must be an integer";
                    }
                    return false;
                }
                record.sourceTimestamp = value;
            }
            auto settings = object.at_key("settings");
            if(settings.error() == simdjson::error_code::SUCCESS)
            {
                std::string_view value;
                if(settings.get_string().get(value) != simdjson::error_code::SUCCESS)
                {
                    if(error)
                    {
                        *error = "Asset manifest settings must be a string";
                    }
                    return false;
                }
                record.settings = value;
            }
            auto dependencies = object.at_key("dependencies");
            if(dependencies.error() == simdjson::error_code::SUCCESS)
            {
                simdjson::dom::array values;
                if(dependencies.get_array().get(values) != simdjson::error_code::SUCCESS)
                {
                    if(error)
                    {
                        *error = "Asset manifest dependencies must be an array";
                    }
                    return false;
                }
                for(const auto dependency : values)
                {
                    std::string_view value;
                    if(dependency.get_string().get(value) != simdjson::error_code::SUCCESS)
                    {
                        if(error)
                        {
                            *error = "Asset manifest dependencies must contain strings";
                        }
                        return false;
                    }
                    record.dependencies.emplace_back(value);
                }
            }
            mEntries[Key(type, relativePath)] = std::move(record);
        }
        return true;
    }

    bool AssetManifest::Save(const std::filesystem::path &resourcesRoot, std::string *error) const
    {
        std::error_code ec;
        std::filesystem::create_directories(resourcesRoot, ec);
        if(ec)
        {
            if(error)
            {
                *error = "Unable to create asset manifest directory: " + ec.message();
            }
            return false;
        }

        std::vector<std::string> keys;
        keys.reserve(mEntries.size());
        for(const auto &[key, entry] : mEntries)
        {
            keys.push_back(key);
        }
        std::ranges::sort(keys);

        std::ofstream file(resourcesRoot / FileName, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            if(error)
            {
                *error = "Unable to write asset manifest: " +
                         (resourcesRoot / FileName).string();
            }
            return false;
        }
        file << "{\n  \"version\": " << FormatVersion::AssetManifest << ",\n  \"assets\": [\n";
        for(std::size_t i = 0; i < keys.size(); ++i)
        {
            const auto &entry = mEntries.at(keys[i]);
            const auto separator = i + 1 < keys.size() ? "," : "";
            file << "    {\"path\":\"" << EscapeJson(entry.relativePath) << "\",\"type\":\""
                 << EscapeJson(entry.type) << "\",\"guid\":\"" << EscapeJson(entry.guid)
                 << "\",\"sourceSize\":" << entry.sourceSize << ",\"sourceHash\":"
                 << entry.sourceHash << ",\"sourceTimestamp\":" << entry.sourceTimestamp
                 << ",\"settings\":\"" << EscapeJson(entry.settings) << "\",\"dependencies\":[";
            for(std::size_t dependency = 0; dependency < entry.dependencies.size(); ++dependency)
            {
                if(dependency != 0)
                {
                    file << ',';
                }
                file << "\"" << EscapeJson(entry.dependencies[dependency]) << "\"";
            }
            file << "]}" << separator << "\n";
        }
        file << "  ]\n}\n";
        return static_cast<bool>(file);
    }

    std::string AssetManifest::GetOrCreate(std::string_view relativePath, std::string_view type)
    {
        const std::string key = Key(type, relativePath);
        if(const auto it = mEntries.find(key); it != mEntries.end())
        {
            return it->second.guid;
        }
        const auto guid = StableGuid(type, relativePath);
        mEntries.emplace(key,
                         ImportRecord {.relativePath = std::string(relativePath),
                                       .type = std::string(type),
                                       .guid = guid});
        return guid;
    }

    std::string AssetManifest::RecordImport(const std::filesystem::path &relativePath,
                                            std::string_view type,
                                            const std::filesystem::path &sourcePath,
                                            std::string settings,
                                            std::vector<std::string> dependencies)
    {
        const auto key = Key(type, relativePath.generic_string());
        auto &record = mEntries[key];
        if(record.guid.empty())
        {
            record.guid = StableGuid(type, relativePath.generic_string());
        }
        record.relativePath = relativePath.generic_string();
        record.type         = type;
        std::error_code ec;
        record.sourceSize = std::filesystem::file_size(sourcePath, ec);
        record.sourceHash = HashFile(sourcePath);
        record.sourceTimestamp = FileTimestamp(sourcePath);
        record.settings = std::move(settings);
        record.dependencies = std::move(dependencies);
        return record.guid;
    }

    const AssetManifest::ImportRecord *AssetManifest::Find(std::string_view relativePath,
                                                           std::string_view type) const
    {
        const auto it = mEntries.find(Key(type, relativePath));
        return it == mEntries.end() ? nullptr : &it->second;
    }

    AssetManifest::ValidationResult
    AssetManifest::Validate(const std::filesystem::path &resourcesRoot) const
    {
        ValidationResult result;
        std::unordered_map<std::string, bool> known;
        for(const auto &[key, record] : mEntries)
        {
            known[record.relativePath] = true;
            const auto path = resourcesRoot / record.relativePath;
            if(!std::filesystem::is_regular_file(path))
            {
                result.missing.push_back(record.relativePath);
                continue;
            }
            if(record.sourceSize != 0 || record.sourceHash != 0)
            {
                std::error_code ec;
                const auto size = std::filesystem::file_size(path, ec);
                if(ec || size != record.sourceSize || HashFile(path) != record.sourceHash)
                {
                    result.changed.push_back(record.relativePath);
                }
            }
        }

        std::error_code ec;
        if(std::filesystem::exists(resourcesRoot, ec))
        {
            for(const auto &entry :
                std::filesystem::recursive_directory_iterator(resourcesRoot, ec))
            {
                if(ec || !entry.is_regular_file(ec) || entry.path().filename() == FileName)
                {
                    continue;
                }
                const auto relative = std::filesystem::relative(entry.path(), resourcesRoot, ec);
                if(!ec && !known.contains(relative.generic_string()))
                {
                    result.orphaned.push_back(relative.generic_string());
                }
            }
        }
        std::ranges::sort(result.missing);
        std::ranges::sort(result.orphaned);
        std::ranges::sort(result.changed);
        return result;
    }

    std::vector<AssetManifest::ImportRecord> AssetManifest::Records() const
    {
        std::vector<ImportRecord> records;
        records.reserve(mEntries.size());
        for(const auto &[key, record] : mEntries)
        {
            records.push_back(record);
        }
        std::ranges::sort(records, [](const auto &left, const auto &right) {
            return left.relativePath < right.relativePath;
        });
        return records;
    }

    void AssetManifest::Clear()
    {
        mRoot.clear();
        mEntries.clear();
    }
} // namespace FRIGGA_NAMESPACE
