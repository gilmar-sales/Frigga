#include <Frigga/Serialization/JsonPretty.hpp>

#include <gtest/gtest.h>

TEST(JsonPretty, FormatsNestedObjectsWithTwoSpaceIndent)
{
    const std::string pretty =
        fg::PrettifyJson(R"({"version":8,"entities":[{"name":"Ground"}]})");
    EXPECT_EQ(pretty,
              "{\n"
              "  \"version\": 8,\n"
              "  \"entities\": [\n"
              "    {\n"
              "      \"name\": \"Ground\"\n"
              "    }\n"
              "  ]\n"
              "}");
}

TEST(JsonPretty, CollapsesEmptyObjectsAndArrays)
{
    EXPECT_EQ(fg::PrettifyJson(R"({"a":{},"b":[]})"),
              "{\n  \"a\": {},\n  \"b\": []\n}");
}

TEST(JsonPretty, PreservesBracesInsideStrings)
{
    const std::string pretty = fg::PrettifyJson(R"({"text":"a{b},c:d\"e"})");
    EXPECT_EQ(pretty, "{\n  \"text\": \"a{b},c:d\\\"e\"\n}");
}

TEST(JsonPretty, ReturnsInvalidJsonUnchanged)
{
    constexpr std::string_view broken = R"({"a":1)";
    EXPECT_EQ(fg::PrettifyJson(broken), broken);
}

TEST(JsonPretty, IsIdempotent)
{
    const std::string once =
        fg::PrettifyJson(R"({"a":[1,2,{"b":true}]})");
    EXPECT_EQ(fg::PrettifyJson(once), once);
}
