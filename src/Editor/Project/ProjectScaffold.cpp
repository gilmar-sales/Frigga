#include "ProjectScaffold.hpp"

#include "ProjectFile.hpp"

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
        return R"cpp(// FRIGGA_MANAGED_GAMEPLAY_SYSTEM
#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

/**
 * Freyr system owned by the gameplay plugin and registered on the host with
 * late DI (ServiceProvider::AddSingleton + SystemManager::RegisterSystem).
 */
class GameplaySystem: public fr::System
{
  public:
    explicit GameplaySystem(const skr::Arc<fr::Registry> &registry);

    void Update(float deltaTime) override;
};
)cpp";
    }

    std::string MakeGameplaySystemCpp()
    {
        return R"cpp(// FRIGGA_MANAGED_GAMEPLAY_SYSTEM
#include "GameplaySystem.hpp"
#include "Components/Health.hpp"

GameplaySystem::GameplaySystem(const skr::Arc<fr::Registry> &registry) : fr::System(registry) {}

void GameplaySystem::Update(float deltaTime)
{
    (void)deltaTime;
    // Example: Freyr Mutation over Name + Health (SoA component from the plugin).
    mRegistry->CreateMutation()->Each<fg::NameComponent, Health>(
        [](fr::Entity, fg::NameComponent &name, Health &health) {
            if(name.name != "Player" && name.name != "Cube")
            {
                return;
            }
            if(health.current > health.max)
            {
                health.current = health.max;
            }
        });
}
)cpp";
    }

    std::string MakeGameplayPluginCpp()
    {
        return R"cpp(// FRIGGA_MANAGED_PLUGIN_ENTRY
#include "GameplaySystem.hpp"
#include "Components/Health.hpp"

#include <frigga_plugin.h>
#include <frigga_user_components.hpp>

#include <Freyr/Core/SystemManager.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

struct FriPlugin
{
    fr::SystemManager *systemManager     = nullptr;
    skr::ServiceProvider *services       = nullptr;
    bool gameplaySystemRegistered        = false;
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
        if(!plugin || !host || !host->registry || !host->user_components ||
           !host->system_manager || !host->services)
        {
            return;
        }

        auto *registry = static_cast<fr::Registry *>(host->registry);
        auto *userComponents =
            static_cast<fg::UserComponentRegistry *>(host->user_components);
        plugin->systemManager = static_cast<fr::SystemManager *>(host->system_manager);
        plugin->services      = static_cast<skr::ServiceProvider *>(host->services);

        FriRegisterUserComponent<Health>(*registry, *userComponents, "Health");

        plugin->services->AddSingleton<GameplaySystem>();
        const auto pipelineId = plugin->systemManager->FindPipelineId("Main");
        if(!pipelineId)
        {
            plugin->services->Remove<GameplaySystem>();
            plugin->systemManager = nullptr;
            plugin->services      = nullptr;
            return;
        }

        plugin->systemManager->RegisterSystem<GameplaySystem>(*pipelineId);
        plugin->gameplaySystemRegistered = true;
    }

    void OnDetach(FriPlugin *plugin)
    {
        if(!plugin)
        {
            return;
        }

        if(plugin->gameplaySystemRegistered && plugin->systemManager)
        {
            (void)plugin->systemManager->UnregisterSystem<GameplaySystem>();
            plugin->gameplaySystemRegistered = false;
        }
        if(plugin->services)
        {
            (void)plugin->services->Remove<GameplaySystem>();
        }
        plugin->systemManager = nullptr;
        plugin->services      = nullptr;
    }

    void OnUpdate(FriPlugin *, float)
    {
        // Gameplay logic runs through Freyr SystemManager (GameplaySystem::Update).
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

    std::string MakeUserComponentsHeader()
    {
        return R"cpp(#pragma once

/**
 * Convenience aliases for project Freyr gameplay components.
 * Types must inherit fr::Component and be registered with FriRegisterUserComponent.
 */

#include <Frigga/ECS/UserComponentReflection.hpp>

using fg::FriRegisterUserComponent;
using fg::FriSet;
using fg::FriTryGet;
)cpp";
    }

    std::string MakeHealthComponentHpp()
    {
        return R"cpp(#pragma once

#include <Freyr/Freyr.hpp>

/// Example project component. Register in GameplayPlugin on_attach.
struct Health: fr::Component
{
    float current = 100.0f;
    float max     = 100.0f;
};
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
        out << "- `src/GameplaySystem.*` — Freyr system (registered on the host via DI)\n";
        out << "- `src/GameplayPlugin.cpp` — C ABI entry for the Editor\n";
        out << "- `src/Components/` — project POD components (example: Health)\n";
        out << "- `include/frigga_user_components.hpp` — FriRegister / FriTryGet / FriSet\n\n";
        out << "## Project components\n\n";
        out << "1. Declare `struct Foo : fr::Component { float x; };`\n";
        out << "2. In `on_attach`: "
               "`FriRegisterUserComponent<Foo>(*registry, *userComponents, \"Foo\");`\n";
        out << "3. Build + **Reload Gameplay Plugin**.\n";
        out << "4. In the Editor: Entity → Add Component → Gameplay → Foo.\n";
        out << "5. In `GameplaySystem::Update`: `CreateMutation()->Each<Foo>(...)` "
               "(runs in the Freyr Main pipeline).\n\n";
        out << "## Gameplay systems\n\n";
        out << "`GameplaySystem` inherits `fr::System` and is registered in `on_attach` with:\n";
        out << "`services->AddSingleton<GameplaySystem>()` then "
               "`systemManager->RegisterSystem<GameplaySystem>(...)`.\n";
        out << "Detach must `UnregisterSystem` + `Remove` before the plugin unloads.\n\n";
        out << "## Build the plugin\n\n";
        out << "Requires a **C++26** compiler with reflection (GCC 16+ or Clang 22+), "
               "same as Frigga.\n\n";
        out << "```bash\n";
        out << "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug "
               "-DCMAKE_CXX_STANDARD=26\n";
        out << "cmake --build build\n";
        out << "```\n\n";
        out << "Or use **File → Build Gameplay Plugin** (Ctrl+B) in the Editor "
               "(forces `gnu++26` + `-freflection`).\n\n";
        out << "The shared library is written to `" << desc.pluginLibraryRelative << "`.\n";
        out << "It resolves Freyr symbols from the Editor process (do not link `libfreyr.a` into the "
               "plugin).\n";
        out << "In the Editor: **File → Build Gameplay Plugin** (Ctrl+B), then **Reload Gameplay "
               "Plugin** "
               "(Ctrl+R), and press Play.\n\n";
        out << "## Debug gameplay code\n\n";
        out << "1. Keep the Frigga Editor open on this project.\n";
        out << "2. Open this folder in VS Code with the Frigga extension.\n";
        out << "3. Run **Frigga: Attach Debugger to Editor** (requires C/C++ extension / GDB).\n";
        out << "4. Set breakpoints in your gameplay sources and hit Play in the Editor.\n";
        return out.str();
    }

    std::string MakeGitignore()
    {
        return "build/\n.frigga/\n";
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

    bool FileContains(const std::filesystem::path &path, std::string_view needle)
    {
        std::ifstream file(path, std::ios::binary);
        if(!file)
        {
            return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str().find(needle) != std::string::npos;
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

    if(!WriteTextFile(projectRoot / ".gitignore", MakeGitignore()))
    {
        result.error = "Failed to write .gitignore";
        return result;
    }

    if(!CopyPluginHeader(desc.friggaRoot, projectRoot))
    {
        result.error = "Failed to copy frigga_plugin.h";
        return result;
    }

    if(!WriteTextFile(projectRoot / "include/frigga_user_components.hpp",
                       MakeUserComponentsHeader()))
    {
        result.error = "Failed to write frigga_user_components.hpp";
        return result;
    }

    result.ok = true;
    return result;
}

bool ProjectScaffold::WriteExampleUserComponents(const std::filesystem::path &projectRoot,
                                                 std::string &error)
{
    // Always refresh the example Health header so migrators pick up fr::Component.
    if(!WriteTextFile(projectRoot / "src/Components/Health.hpp", MakeHealthComponentHpp()))
    {
        error = "Failed to write src/Components/Health.hpp";
        return false;
    }
    return true;
}

bool ProjectScaffold::MaybeRewriteManagedPluginEntry(const std::filesystem::path &projectRoot,
                                                     std::string &error)
{
    const auto pluginPath = projectRoot / "src/GameplayPlugin.cpp";
    const bool exists     = std::filesystem::exists(pluginPath);
    if(exists && !FileContains(pluginPath, ManagedPluginMarker))
    {
        return true;
    }
    if(!WriteTextFile(pluginPath, MakeGameplayPluginCpp()))
    {
        error = "Failed to write src/GameplayPlugin.cpp";
        return false;
    }
    return true;
}

bool ProjectScaffold::MaybeRewriteManagedGameplaySystem(const std::filesystem::path &projectRoot,
                                                        std::string &error)
{
    const auto hppPath = projectRoot / "src/GameplaySystem.hpp";
    const auto cppPath = projectRoot / "src/GameplaySystem.cpp";
    const bool hppExists = std::filesystem::exists(hppPath);
    const bool cppExists = std::filesystem::exists(cppPath);

    const bool managedHpp =
        !hppExists || FileContains(hppPath, ManagedGameplaySystemMarker) ||
        FileContains(hppPath, "GameplaySystem(fr::Registry *registry)");
    const bool managedCpp =
        !cppExists || FileContains(cppPath, ManagedGameplaySystemMarker) ||
        FileContains(cppPath, "GameplaySystem::GameplaySystem(fr::Registry *registry)");

    if(hppExists && cppExists && !(managedHpp && managedCpp))
    {
        // Customized by the user — leave alone.
        return true;
    }

    if(!WriteTextFile(hppPath, MakeGameplaySystemHpp()) ||
       !WriteTextFile(cppPath, MakeGameplaySystemCpp()))
    {
        error = "Failed to write src/GameplaySystem.*";
        return false;
    }
    return true;
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

    std::string exampleError;
    if(!WriteExampleUserComponents(projectRoot, exampleError))
    {
        result.error = exampleError;
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
