#pragma once

#include "ProjectDescriptor.hpp"

#include <filesystem>
#include <optional>
#include <string>

class ProjectFile
{
  public:
    static constexpr const char *FileName = "frigga.project";

    static bool Save(const std::filesystem::path &projectFile, const ProjectDescriptor &desc);
    static std::optional<ProjectDescriptor> Load(const std::filesystem::path &projectFile);
};
