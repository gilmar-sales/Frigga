#pragma once

#include <filesystem>
#include <string>

/// OpenCode (`opencode.json`) MCP config for gameplay projects.
///
/// The file also carries user keys (model, permissions, other servers), so it
/// is merged, never overwritten: only the `frigga-editor` entry under `"mcp"`
/// is inserted when missing.
class OpenCodeMcp
{
  public:
    static bool Ensure(const std::filesystem::path &projectRoot, std::string &error);
};
