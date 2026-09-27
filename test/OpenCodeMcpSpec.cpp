#include "Editor/Project/OpenCodeMcp.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    std::filesystem::path MakeTempRoot()
    {
        return std::filesystem::temp_directory_path() /
               ("frigga-opencode-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    }

    std::string ReadFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void WriteFile(const std::filesystem::path &path, std::string_view contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << contents;
    }

    // Minimal brace-aware JSON sanity check (balanced + no dangling commas).
    bool LooksLikeValidJson(const std::string &text)
    {
        int depth          = 0;
        bool inString      = false;
        bool escape        = false;
        char lastSignificant = '\0';
        for(const char ch : text)
        {
            if(inString)
            {
                if(escape)
                {
                    escape = false;
                }
                else if(ch == '\\')
                {
                    escape = true;
                }
                else if(ch == '"')
                {
                    inString = false;
                }
                continue;
            }
            if(ch == '"')
            {
                inString = true;
            }
            else if(ch == '{' || ch == '[')
            {
                ++depth;
                lastSignificant = ch;
            }
            else if(ch == '}' || ch == ']')
            {
                if(lastSignificant == '{' || lastSignificant == '[' || lastSignificant == ',')
                {
                    return false;
                }
                --depth;
                if(depth < 0)
                {
                    return false;
                }
                lastSignificant = ch;
            }
            else if(!std::isspace(static_cast<unsigned char>(ch)))
            {
                lastSignificant = ch;
            }
        }
        return !inString && depth == 0;
    }
} // namespace

TEST(OpenCodeMcp, CreatesDefaultWhenMissing)
{
    const auto root = MakeTempRoot();
    std::filesystem::create_directories(root);

    std::string error;
    ASSERT_TRUE(OpenCodeMcp::Ensure(root, error)) << error;
    const auto text = ReadFile(root / "opencode.json");
    EXPECT_NE(text.find("frigga-editor"), std::string::npos);
    EXPECT_NE(text.find("\"type\": \"local\""), std::string::npos);
    EXPECT_TRUE(LooksLikeValidJson(text));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(OpenCodeMcp, MergesIntoExistingWithoutMcpKey)
{
    const auto root = MakeTempRoot();
    std::filesystem::create_directories(root);
    WriteFile(root / "opencode.json",
              "{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"model\": \"foo/bar\"\n}\n");

    std::string error;
    ASSERT_TRUE(OpenCodeMcp::Ensure(root, error)) << error;
    const auto text = ReadFile(root / "opencode.json");
    EXPECT_NE(text.find("\"model\": \"foo/bar\""), std::string::npos);
    EXPECT_NE(text.find("frigga-editor"), std::string::npos);
    EXPECT_TRUE(LooksLikeValidJson(text));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(OpenCodeMcp, MergesIntoEmptyMcpObject)
{
    const auto root = MakeTempRoot();
    std::filesystem::create_directories(root);
    WriteFile(root / "opencode.json", "{ \"mcp\": {} }\n");

    std::string error;
    ASSERT_TRUE(OpenCodeMcp::Ensure(root, error)) << error;
    const auto text = ReadFile(root / "opencode.json");
    EXPECT_NE(text.find("frigga-editor"), std::string::npos);
    EXPECT_TRUE(LooksLikeValidJson(text));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(OpenCodeMcp, PreservesOtherServersInNonEmptyMcp)
{
    const auto root = MakeTempRoot();
    std::filesystem::create_directories(root);
    WriteFile(root / "opencode.json",
              "{ \"mcp\": { \"other\": { \"type\": \"local\", \"command\": [\"other\"] } } }\n");

    std::string error;
    ASSERT_TRUE(OpenCodeMcp::Ensure(root, error)) << error;
    const auto text = ReadFile(root / "opencode.json");
    EXPECT_NE(text.find("\"other\""), std::string::npos);
    EXPECT_NE(text.find("frigga-editor"), std::string::npos);
    EXPECT_TRUE(LooksLikeValidJson(text));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(OpenCodeMcp, LeavesConfiguredFileUntouched)
{
    const auto root = MakeTempRoot();
    std::filesystem::create_directories(root);
    static constexpr std::string_view kExisting =
        "{ \"mcp\": { \"frigga-editor\": { \"type\": \"local\" } } }\n";
    WriteFile(root / "opencode.json", kExisting);

    std::string error;
    ASSERT_TRUE(OpenCodeMcp::Ensure(root, error)) << error;
    EXPECT_EQ(ReadFile(root / "opencode.json"), kExisting);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}
