#include "OpenCodeMcp.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

namespace
{
    bool WriteTextFile(const std::filesystem::path &path, std::string_view contents)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            return false;
        }
        file << contents;
        return static_cast<bool>(file);
    }

    std::string ReadTextFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if(!file)
        {
            return {};
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Returns the merged text, or empty when the file already contains the
    // entry (or cannot be merged safely — the caller's file is never clobbered).
    std::string MergeMcpEntry(const std::string &existing)
    {
        if(existing.find("frigga-editor") != std::string::npos)
        {
            return {};
        }
        static constexpr std::string_view kEntry =
            "    \"frigga-editor\": {\n"
            "      \"type\": \"local\",\n"
            "      \"command\": [\"python3\", \"tools/frigga-mcp/server.py\"],\n"
            "      \"enabled\": true\n"
            "    }";
        const auto mcpPos = existing.find("\"mcp\"");
        if(mcpPos == std::string::npos)
        {
            const auto open  = existing.find('{');
            const auto close = existing.rfind('}');
            if(open == std::string::npos || close == std::string::npos || close <= open)
            {
                return {};
            }
            bool empty = true;
            for(std::size_t i = open + 1; i < close; ++i)
            {
                if(!std::isspace(static_cast<unsigned char>(existing[i])))
                {
                    empty = false;
                    break;
                }
            }
            std::string merged = existing;
            std::string block  = empty ? "  \"mcp\": {\n" : ",\n  \"mcp\": {\n";
            block += kEntry;
            block += "\n  }\n";
            merged.insert(close, block);
            return merged;
        }
        const auto brace = existing.find('{', mcpPos);
        if(brace == std::string::npos)
        {
            return {};
        }
        std::size_t cursor = brace + 1;
        while(cursor < existing.size() &&
              std::isspace(static_cast<unsigned char>(existing[cursor])))
        {
            ++cursor;
        }
        std::string merged = existing;
        if(cursor < existing.size() && existing[cursor] == '}')
        {
            merged.insert(cursor, "\n" + std::string(kEntry) + "\n  ");
            return merged;
        }
        merged.insert(brace + 1, "\n" + std::string(kEntry) + ",");
        return merged;
    }
} // namespace

bool OpenCodeMcp::Ensure(const std::filesystem::path &projectRoot, std::string &error)
{
    static constexpr std::string_view kOpenCodeJson =
        "{\n"
        "  \"$schema\": \"https://opencode.ai/config.json\",\n"
        "  \"mcp\": {\n"
        "    \"frigga-editor\": {\n"
        "      \"type\": \"local\",\n"
        "      \"command\": [\"python3\", \"tools/frigga-mcp/server.py\"],\n"
        "      \"enabled\": true\n"
        "    }\n"
        "  }\n"
        "}\n";
    const auto path = projectRoot / "opencode.json";
    if(!std::filesystem::exists(path))
    {
        if(!WriteTextFile(path, kOpenCodeJson))
        {
            error = "Failed to write opencode.json";
            return false;
        }
        return true;
    }
    const std::string existing = ReadTextFile(path);
    if(existing.empty())
    {
        if(!WriteTextFile(path, kOpenCodeJson))
        {
            error = "Failed to write opencode.json";
            return false;
        }
        return true;
    }
    const std::string merged = MergeMcpEntry(existing);
    if(merged.empty())
    {
        return true;
    }
    if(!WriteTextFile(path, merged))
    {
        error = "Failed to write opencode.json";
        return false;
    }
    return true;
}
