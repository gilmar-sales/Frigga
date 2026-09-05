#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct RuntimeModule
{
    std::string id;
    std::string name;
    std::filesystem::path library;
    bool enabled = true;
};

struct RuntimeProject
{
    std::string name;
    std::string displayName;
    std::string executableName;
    std::string publisher;
    std::string copyright;
    std::string version = "1.0.0";
    std::string identifier;
    std::filesystem::path root;
    std::filesystem::path scene = "scenes/main.json";
    std::vector<RuntimeModule> modules;

    [[nodiscard]] std::filesystem::path ScenePath() const
    {
        return root / scene;
    }

    [[nodiscard]] static bool Load(const std::filesystem::path &projectFile,
                                   RuntimeProject &project, std::string &error);
};
