#pragma once

#include "ProjectDescriptor.hpp"

#include <Frigga/Scene/Scene.hpp>

#include <filesystem>
#include <string>

struct ProjectScaffoldResult
{
    bool ok = false;
    std::filesystem::path projectFile;
    std::string error;
};

class ProjectScaffold
{
  public:
    /// Creates project directory under parentDir/name and writes all scaffold files.
    /// Uses @p scene to seed and serialize the template scene JSON.
    static ProjectScaffoldResult Create(const std::filesystem::path &parentDir,
                                        const ProjectDescriptor &desc,
                                        fg::Scene &scene);
};
