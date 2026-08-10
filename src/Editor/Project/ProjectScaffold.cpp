#include "ProjectScaffold.hpp"

#include "ProjectFile.hpp"

#include <fstream>
#include <sstream>

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

    std::string DefaultPluginLibraryRelative(const std::string &target)
    {
#ifdef _WIN32
        return "build/" + target + ".dll";
#elif defined(__APPLE__)
        return "build/lib" + target + ".dylib";
#else
        return "build/lib" + target + ".so";
#endif
    }

    std::string MakeCMakeLists(const ProjectDescriptor &desc)
    {
        std::ostringstream out;
        out << "cmake_minimum_required(VERSION 3.29)\n";
        out << "project(" << desc.name << "Gameplay LANGUAGES CXX)\n\n";
        out << "# Frigga gameplay plugins require C++26 (+ reflection), matching the Editor.\n";
        out << "set(CMAKE_CXX_STANDARD 26)\n";
        out << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
        out << "set(CMAKE_CXX_EXTENSIONS ON)\n";
        out << "set(CMAKE_POSITION_INDEPENDENT_CODE ON)\n\n";
        out << "if(CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\")\n";
        out << "  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)\n";
        out << "    message(FATAL_ERROR \"Gameplay plugins need GCC 16+ (C++26 reflection). "
               "Found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\")\n";
        out << "  endif()\n";
        out << "elseif(CMAKE_CXX_COMPILER_ID MATCHES \"Clang\")\n";
        out << "  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)\n";
        out << "    message(FATAL_ERROR \"Gameplay plugins need Clang 22+ (C++26 reflection). "
               "Found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\")\n";
        out << "  endif()\n";
        out << "endif()\n\n";
        out << "if(NOT DEFINED FRIGGA_ROOT)\n";
        out << "  set(FRIGGA_ROOT \"" << desc.friggaRoot.generic_string() << "\")\n";
        out << "endif()\n";
        out << "if(NOT DEFINED FRIGGA_BUILD)\n";
        out << "  set(FRIGGA_BUILD \"" << desc.friggaBuild.generic_string() << "\")\n";
        out << "endif()\n\n";
        out << "set(FREYR_INCLUDE \"${FRIGGA_BUILD}/_deps/freyr-src/include\")\n";
        out << "set(SKIRNIR_INCLUDE \"${FRIGGA_BUILD}/_deps/skirnir-src/include\")\n";
        out << "set(GLM_INCLUDE \"${FRIGGA_BUILD}/_deps/glm-src\")\n";
        out << "set(FREYR_LIB_DIR \"${FRIGGA_BUILD}/_deps/freyr-build\")\n";
        out << "set(SKIRNIR_LIB_DIR \"${FRIGGA_BUILD}/_deps/skirnir-build\")\n\n";
        out << "add_library(" << desc.pluginTarget << " SHARED\n";
        out << "  src/GameplayPlugin.cpp\n";
        out << "  src/GameplaySystem.cpp\n";
        out << ")\n\n";
        out << "target_compile_features(" << desc.pluginTarget << " PRIVATE cxx_std_26)\n";
        out << "target_compile_definitions(" << desc.pluginTarget << " PRIVATE FRI_PLUGIN_EXPORTS)\n";
        out << "if(MSVC)\n";
        out << "  target_compile_options(" << desc.pluginTarget
           << " PRIVATE /std:c++latest /experimental:reflection)\n";
        out << "else()\n";
        out << "  # Force the dialect Freyr/Skirnir need (gnu++26 + reflection).\n";
        out << "  target_compile_options(" << desc.pluginTarget
           << " PRIVATE -std=gnu++26 -freflection)\n";
        out << "endif()\n";
        out << "target_include_directories(" << desc.pluginTarget << " PRIVATE\n";
        out << "  ${CMAKE_CURRENT_SOURCE_DIR}/include\n";
        out << "  ${CMAKE_CURRENT_SOURCE_DIR}/src\n";
        out << "  ${FRIGGA_ROOT}/src\n";
        out << "  ${FREYR_INCLUDE}\n";
        out << "  ${SKIRNIR_INCLUDE}\n";
        out << "  ${GLM_INCLUDE}\n";
        out << ")\n\n";
        out << "find_package(Threads REQUIRED)\n";
        out << "# Freyr/Skirnir are linked into the Editor (static). The plugin resolves\n";
        out << "# those symbols from the Editor process (requires --export-dynamic).\n";
        out << "# Do not link libfreyr.a into this SHARED lib (TLS local-exec / not -fPIC-safe).\n";
        out << "target_link_libraries(" << desc.pluginTarget << " PRIVATE Threads::Threads)\n";
        out << "if(UNIX AND NOT APPLE)\n";
        out << "  target_link_options(" << desc.pluginTarget << " PRIVATE -Wl,--allow-shlib-undefined)\n";
        out << "endif()\n";
        out << "set_target_properties(" << desc.pluginTarget << " PROPERTIES\n";
        out << "  CXX_STANDARD 26\n";
        out << "  CXX_STANDARD_REQUIRED ON\n";
        out << "  CXX_EXTENSIONS ON\n";
        out << "  LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/build\"\n";
        out << "  RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/build\"\n";
        out << "  ARCHIVE_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/build\"\n";
        out << ")\n";
        return out.str();
    }

    std::string MakeGameplaySystemHpp()
    {
        return R"cpp(#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Frigga/Macro.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>

#include <Freyr/Freyr.hpp>

/**
 * Gameplay logic runs inside the loaded shared library.
 * This type mirrors fr::System but is NOT registered with Freyr — the Editor
 * bridge calls Update() through the C plugin API while play mode is running.
 *
 * Only engine-registered components (Name, Transform, Mesh, …) may be used.
 */
class GameplaySystem
{
  public:
    explicit GameplaySystem(fr::Registry *registry);

    void OnAttach();
    void OnDetach();
    void Update(float deltaTime);

  private:
    fr::Registry *mRegistry = nullptr;
};
)cpp";
    }

    std::string MakeGameplaySystemCpp()
    {
        return R"cpp(#include "GameplaySystem.hpp"

GameplaySystem::GameplaySystem(fr::Registry *registry) : mRegistry(registry) {}

void GameplaySystem::OnAttach() {}

void GameplaySystem::OnDetach() {}

void GameplaySystem::Update(float deltaTime)
{
    if(!mRegistry)
    {
        return;
    }

    // Example: nudge entities named "Player" along +X (edit for your game).
    mRegistry->CreateMutation()->Each<fg::NameComponent, fg::TransformComponent>(
        [deltaTime](fr::Entity, fg::NameComponent &name, fg::TransformComponent &transform) {
            if(name.name != "Player" && name.name != "Cube")
            {
                return;
            }
            transform.position.x += 0.5f * deltaTime;
        });
}
)cpp";
    }

    std::string MakeGameplayPluginCpp()
    {
        return R"cpp(#include "GameplaySystem.hpp"

#include <frigga_plugin.h>

#include <Freyr/Freyr.hpp>

#include <memory>

struct FriPlugin
{
    std::unique_ptr<GameplaySystem> system;
};

namespace
{
    FriPlugin *CreatePlugin()
    {
        return new FriPlugin();
    }

    void DestroyPlugin(FriPlugin *plugin)
    {
        delete plugin;
    }

    void OnAttach(FriPlugin *plugin, const FriHost *host)
    {
        if(!plugin || !host || !host->registry)
        {
            return;
        }
        auto *registry = static_cast<fr::Registry *>(host->registry);
        plugin->system = std::make_unique<GameplaySystem>(registry);
        plugin->system->OnAttach();
    }

    void OnDetach(FriPlugin *plugin)
    {
        if(!plugin || !plugin->system)
        {
            return;
        }
        plugin->system->OnDetach();
        plugin->system.reset();
    }

    void OnUpdate(FriPlugin *plugin, float deltaTime)
    {
        if(plugin && plugin->system)
        {
            plugin->system->Update(deltaTime);
        }
    }

    const FriPluginApi kApi {
        .create     = CreatePlugin,
        .destroy    = DestroyPlugin,
        .on_attach  = OnAttach,
        .on_detach  = OnDetach,
        .on_update  = OnUpdate,
    };
} // namespace

extern "C" FRI_PLUGIN_API const FriPluginApi *fri_plugin_api(void)
{
    return &kApi;
}
)cpp";
    }

    std::string MakeReadme(const ProjectDescriptor &desc)
    {
        std::ostringstream out;
        out << "# " << desc.name << "\n\n";
        out << "Frigga gameplay project (" << desc.TemplateId() << " template).\n\n";
        out << "## Layout\n\n";
        out << "- `frigga.project` — project metadata\n";
        out << "- `scenes/main.json` — default scene\n";
        out << "- `src/GameplaySystem.*` — Freyr-style gameplay logic\n";
        out << "- `src/GameplayPlugin.cpp` — C ABI entry for the Editor\n\n";
        out << "## Build the plugin\n\n";
        out << "Requires a **C++26** compiler with reflection (GCC 16+ or Clang 22+), same as Frigga.\n\n";
        out << "```bash\n";
        out << "cmake -S . -B build-plugin -G Ninja -DCMAKE_BUILD_TYPE=Debug "
               "-DCMAKE_CXX_STANDARD=26\n";
        out << "cmake --build build-plugin\n";
        out << "```\n\n";
        out << "Or use **File → Build Gameplay Plugin** in the Editor "
               "(forces `gnu++26` + `-freflection`).\n\n";
        out << "The shared library is written to `" << desc.pluginLibraryRelative << "`.\n";
        out << "It resolves Freyr symbols from the Editor process (do not link `libfreyr.a` into the "
               "plugin).\n";
        out << "In the Editor: **File → Build Gameplay Plugin**, then **Reload Gameplay Plugin** "
               "(Ctrl+R), and press Play.\n\n";
        out << "## Constraints\n\n";
        out << "Use only components already registered by Frigga "
               "(`Transform`, `Name`, `Mesh`, …). "
               "Custom component types require engine bootstrap registration.\n";
        return out.str();
    }

    bool CopyPluginHeader(const std::filesystem::path &friggaRoot,
                          const std::filesystem::path &projectRoot)
    {
        const auto src = friggaRoot / "src/Frigga/Plugin/frigga_plugin.h";
        const auto dst = projectRoot / "include/frigga_plugin.h";
        std::error_code ec;
        std::filesystem::create_directories(dst.parent_path(), ec);
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        return !ec;
    }
} // namespace

ProjectManagedWriteResult ProjectScaffold::WriteManagedFiles(
    const std::filesystem::path &projectRoot, const ProjectDescriptor &desc)
{
    ProjectManagedWriteResult result;
    if(desc.friggaRoot.empty() || desc.friggaBuild.empty())
    {
        result.error = "Engine paths (friggaRoot / friggaBuild) are required";
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(projectRoot / "include", ec);
    if(ec)
    {
        result.error = "Failed to create include/";
        return result;
    }

    if(!WriteTextFile(projectRoot / "CMakeLists.txt", MakeCMakeLists(desc)))
    {
        result.error = "Failed to write CMakeLists.txt";
        return result;
    }

    if(!WriteTextFile(projectRoot / "README.md", MakeReadme(desc)))
    {
        result.error = "Failed to write README.md";
        return result;
    }

    if(!CopyPluginHeader(desc.friggaRoot, projectRoot))
    {
        result.error = "Failed to copy frigga_plugin.h";
        return result;
    }

    result.ok = true;
    return result;
}

ProjectScaffoldResult ProjectScaffold::Create(const std::filesystem::path &parentDir,
                                              const ProjectDescriptor &descIn,
                                              fg::Scene &scene)
{
    ProjectScaffoldResult result;
    ProjectDescriptor desc = descIn;
    desc.formatVersion     = ProjectDescriptor::CurrentFormatVersion;

    if(desc.name.empty())
    {
        result.error = "Project name is empty";
        return result;
    }
    if(desc.friggaRoot.empty() || desc.friggaBuild.empty())
    {
        result.error = "Engine paths (friggaRoot / friggaBuild) are required";
        return result;
    }

    if(desc.pluginLibraryRelative.empty())
    {
        desc.pluginLibraryRelative = DefaultPluginLibraryRelative(desc.pluginTarget);
    }

    const auto projectRoot = parentDir / desc.name;
    std::error_code ec;
    if(std::filesystem::exists(projectRoot))
    {
        result.error = "Project directory already exists: " + projectRoot.string();
        return result;
    }

    std::filesystem::create_directories(projectRoot / "src", ec);
    std::filesystem::create_directories(projectRoot / "scenes", ec);
    std::filesystem::create_directories(projectRoot / "include", ec);
    std::filesystem::create_directories(projectRoot / "build", ec);
    if(ec)
    {
        result.error = "Failed to create project directories";
        return result;
    }

    const auto projectFile = projectRoot / ProjectFile::FileName;
    if(!ProjectFile::Save(projectFile, desc))
    {
        result.error = "Failed to write frigga.project";
        return result;
    }

    const auto managed = WriteManagedFiles(projectRoot, desc);
    if(!managed.ok)
    {
        result.error = managed.error;
        return result;
    }

    if(!WriteTextFile(projectRoot / "src/GameplaySystem.hpp", MakeGameplaySystemHpp()) ||
       !WriteTextFile(projectRoot / "src/GameplaySystem.cpp", MakeGameplaySystemCpp()) ||
       !WriteTextFile(projectRoot / "src/GameplayPlugin.cpp", MakeGameplayPluginCpp()))
    {
        result.error = "Failed to write scaffold source files";
        return result;
    }

    scene.NewSceneFromTemplate(desc.sceneTemplate);
    const auto scenePath = projectRoot / desc.sceneRelativePath;
    if(!scene.SaveScene(scenePath))
    {
        result.error = "Failed to write scene JSON";
        return result;
    }

    result.ok          = true;
    result.projectFile = projectFile;
    return result;
}
