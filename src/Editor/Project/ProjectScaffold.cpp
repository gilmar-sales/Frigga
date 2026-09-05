#include "ProjectScaffold.hpp"

#include "ModuleCatalog.hpp"
#include "ProjectEnginePaths.hpp"
#include "ProjectFile.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Input/InputMap.hpp>
#include <Frigga/Input/InputMapIO.hpp>

#include <array>
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

    std::string DefaultModuleLibraryRelative(const std::string &target)
    {
#ifdef _WIN32
        return "build/" + target + ".dll";
#elif defined(__APPLE__)
        return "build/lib" + target + ".dylib";
#else
        return "build/lib" + target + ".so";
#endif
    }

    std::string EscapeJson(std::string_view value)
    {
        std::ostringstream out;
        for(const char ch : value)
        {
            switch(ch)
            {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            default:
                out << ch;
                break;
            }
        }
        return out.str();
    }

    std::string MakeCMakeUserPresets(const ProjectDescriptor &desc)
    {
        const auto sdk   = EffectiveFriggaSdk(desc).generic_string();
        const auto build = EffectiveFriggaBuild(desc).generic_string();
        auto runtimeBase = EffectiveFriggaBuild(desc);
        if(runtimeBase.filename() == "Sdk")
        {
            runtimeBase = runtimeBase.parent_path();
        }
#ifdef _WIN32
        const auto runtime = (runtimeBase / "Runtime.exe").generic_string();
#else
        const auto runtime = (runtimeBase / "Runtime").generic_string();
#endif
        std::ostringstream out;
        out << "{\n";
        out << "  \"version\": 6,\n";
        out << "  \"cmakeMinimumRequired\": { \"major\": 3, \"minor\": 29, \"patch\": 0 },\n";
        out << "  \"configurePresets\": [\n";
        out << "    {\n";
        out << "      \"name\": \"default\",\n";
        out << "      \"displayName\": \"Frigga (local Editor)\",\n";
        out << "      \"generator\": \"Ninja\",\n";
        out << "      \"binaryDir\": \"${sourceDir}/build\",\n";
        out << "      \"cacheVariables\": {\n";
        out << "        \"CMAKE_BUILD_TYPE\": \"Debug\",\n";
        out << "        \"CMAKE_CXX_STANDARD\": \"26\",\n";
        out << "        \"FRIGGA_SDK\": \"" << EscapeJson(sdk) << "\",\n";
        out << "        \"FRIGGA_BUILD\": \"" << EscapeJson(build) << "\",\n";
        out << "        \"FRIGGA_RUNTIME\": \"" << EscapeJson(runtime) << "\"\n";
        out << "      }\n";
        out << "    }\n";
        out << "  ]\n";
        out << "}\n";
        return out.str();
    }

    std::string MakeManagedModuleSubdirsBlock(const ProjectDescriptor &desc)
    {
        std::ostringstream out;
        out << ProjectScaffold::ManagedModuleSubdirsBegin << "\n";
        for(const auto &entry : desc.modules)
        {
            const auto folder = entry.id.empty() ? entry.target : entry.id;
            out << "if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/"
                << ProjectDescriptor::ModulesDirName << "/" << folder
                << "/CMakeLists.txt\")\n";
            out << "  add_subdirectory(" << ProjectDescriptor::ModulesDirName << "/" << folder
                << ")\n";
            out << "endif()\n";
        }
        out << ProjectScaffold::ManagedModuleSubdirsEnd << "\n";
        return out.str();
    }

    std::string MakeCMakeLists(const ProjectDescriptor &desc)
    {
        std::ostringstream out;
        out << "cmake_minimum_required(VERSION 3.29)\n";
        out << "project(" << desc.name << "Gameplay LANGUAGES CXX)\n\n";
        out << "set(CMAKE_CXX_STANDARD 26)\n";
        out << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
        out << "set(CMAKE_CXX_EXTENSIONS ON)\n";
        out << "set(CMAKE_POSITION_INDEPENDENT_CODE ON)\n";
        out << "set(CMAKE_CXX_SCAN_FOR_MODULES 0)\n\n";
        out << "set(FRIGGA_SDK \"\" CACHE PATH \"Frigga SDK or source tree\")\n";
        out << "set(FRIGGA_RUNTIME \"\" CACHE FILEPATH \"Frigga Runtime executable\")\n";
        out << "if(NOT FRIGGA_SDK AND DEFINED ENV{FRIGGA_SDK} AND NOT \"$ENV{FRIGGA_SDK}\" STREQUAL \"\")\n";
        out << "  set(FRIGGA_SDK \"$ENV{FRIGGA_SDK}\" CACHE PATH \"Frigga SDK or source tree\" FORCE)\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_SDK)\n";
        out << "  message(FATAL_ERROR \"Frigga SDK not found. Configure with -DFRIGGA_SDK=<path>, set FRIGGA_SDK, or use CMakeUserPresets.json.\")\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_RUNTIME AND DEFINED ENV{FRIGGA_RUNTIME} AND NOT \"$ENV{FRIGGA_RUNTIME}\" STREQUAL \"\")\n";
        out << "  set(FRIGGA_RUNTIME \"$ENV{FRIGGA_RUNTIME}\" CACHE FILEPATH \"Frigga Runtime executable\" FORCE)\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_RUNTIME)\n";
        out << "  message(FATAL_ERROR \"Frigga Runtime not found. Configure with -DFRIGGA_RUNTIME=<path> or set FRIGGA_RUNTIME.\")\n";
        out << "endif()\n";
        out << "if(NOT EXISTS \"${FRIGGA_SDK}/cmake/FriggaSdk.cmake\")\n";
        out << "  message(FATAL_ERROR \"Invalid Frigga SDK: ${FRIGGA_SDK}/cmake/FriggaSdk.cmake not found\")\n";
        out << "endif()\n";
        out << "include(\"${FRIGGA_SDK}/cmake/FriggaSdk.cmake\")\n\n";
        out << "frigga_install_game(\"${FRIGGA_RUNTIME}\" \"" << desc.name << "\")\n\n";
        out << MakeManagedModuleSubdirsBlock(desc);
        return out.str();
    }

    std::string MakeGameplaySystemHpp()
    {
        return R"cpp(// FRIGGA_MANAGED_GAMEPLAY_SYSTEM
#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

/**
 * Freyr system owned by the gameplay module.
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
#include "systems/GameplaySystem.hpp"
#include "components/Health.hpp"

GameplaySystem::GameplaySystem(const skr::Arc<fr::Registry> &registry) : fr::System(registry)
{
}

void GameplaySystem::Update(float)
{
    mRegistry->CreateMutation()->Each(
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

    std::string MakeGameplayModuleCpp()
    {
        return R"cpp(// FRIGGA_MANAGED_MODULE_ENTRY
#include "systems/GameplaySystem.hpp"
#include "components/Health.hpp"

#include <Frigga/Module/FriModule.hpp>

FRI_MODULE(module)
{
    module.Component<Health>()
          .System<GameplaySystem>();
}
)cpp";
    }

    std::string MakeGameplayModuleCMake()
    {
        return "frigga_add_module(gameplay\n"
               "  src/GameplayModule.cpp\n"
               "  src/systems/GameplaySystem.cpp\n"
               ")\n";
    }

    std::string MakeUserComponentsHeader()
    {
        return R"cpp(#pragma once

/**
 * Convenience aliases for project Freyr gameplay components.
 * Register types with FRI_MODULE: module.Component<T>().
 */

#include <Frigga/ECS/UserComponentReflection.hpp>

using fg::FriSet;
using fg::FriTryGet;
)cpp";
    }

    std::string MakeHealthComponentHpp()
    {
        return R"cpp(#pragma once

#include <Freyr/Freyr.hpp>

/// Example project component. Register in GameplayModule on_attach.
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
        out << "- `input.json` — named Actions / Axes bindings\n";
        out << "- `ecs.json` — ECS pipeline / system layout (created on first Editor open)\n";
        out << "- `scenes/main.json` — default scene\n";
        out << "- `Resources/` — models, textures, prefabs, and fonts owned by this project\n";
        out << "- `modules/` — shared libraries (`gameplay`, optional extras)\n";
        out << "- `modules/gameplay/src/systems/` — Freyr systems\n";
        out << "- `modules/gameplay/src/components/` — project POD components (example: Health)\n";
        out << "- `modules/gameplay/src/GameplayModule.cpp` — FRI_MODULE entry\n";
        out << "- `include/frigga_user_components.hpp` — FriSet / FriTryGet helpers\n\n";
        out << "## Project components\n\n";
        out << "1. Declare `struct Foo : fr::Component { float x; };`\n";
        out << "2. In `FRI_MODULE`: `module.Component<Foo>()`\n";
        out << "3. Build + **Reload Gameplay Module**.\n";
        out << "4. In the Editor: Entity → Add Component → Gameplay → Foo.\n";
        out << "5. In a Freyr `System::Update`: `CreateMutation()->Each([](fr::Entity, Foo& foo) { ... })` "
               "(Simulation pipeline — Play mode only).\n\n";
        out << "## Gameplay systems\n\n";
        out << "Inherit `fr::System` and register with `module.System<MySystem>()` "
               "(defaults to the **Simulation** pipeline).\n";
        out << "Optional DI: `module.Singleton<T>()`, `.Scoped<T>()`, `.Transient<T>()`.\n";
        out << "Host exposes `fg::Input`, `fg::Physics`, and `fg::AudioController` — inject them "
               "in system ctors "
               "(`IsDown`/`WasPressed`/`GetAxis`, `MoveCharacter`/`SetLinearVelocity`, "
               "`Play`/`Stop` on entities with `AudioSourceComponent`).\n";
        out << "Host placement: new module systems append to **Simulation** (60 Hz, Play only); "
               "known labels are restored from `ecs.json` after attach. Edit pipelines in the "
               "**ECS** workflow.\n";
        out << "Tick order: **Simulation** (gameplay + physics) → **Main** (audio + camera) → "
               "**Render** (animation preview + draw, always last). Edit mode keeps only "
               "Render; Simulation and Main tick in Play.\n\n";
        out << "## Build the module\n\n";
        out << "Requires a **C++26** compiler with reflection (GCC 16+ or Clang 22+), "
               "same as Frigga.\n\n";
        out << "Point CMake at the packaged self-contained `Sdk/` next to the Editor "
               "(or the engine tree):\n\n";
        out << "```bash\n";
        out << "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug "
               "-DFRIGGA_SDK=/path/to/Sdk\n";
        out << "cmake --build build\n";
        out << "```\n\n";
        out << "Alternatively set the `FRIGGA_SDK` environment variable, or use "
               "`cmake --preset default` after the Editor has written local "
               "`CMakeUserPresets.json` (gitignored).\n";
        out << "The SDK contains `include/Frigga`, dependency headers, and the shared "
               "`cmake/FriggaSdk.cmake` module helper. Engine developers can point "
               "`FRIGGA_SDK` at the source tree and optionally pass `-DFRIGGA_BUILD=` "
               "(Editor binary dir with `_deps/`) for local dependency headers.\n\n";
        out << "Or use **File → Build Gameplay Module** (Ctrl+B) in the Editor "
               "(passes SDK paths and forces `gnu++26` + `-freflection`).\n\n";
        out << "The shared library is written to `" << desc.moduleLibraryRelative << "`.\n";
        out << "It resolves Freyr symbols from the Editor process (do not link `libfreyr.a` into the "
               "module).\n";
        out << "In the Editor: **File → Build Gameplay Module** (Ctrl+B), then **Reload Gameplay "
               "Module** "
               "(Ctrl+R), and press Play.\n\n";
        out << "## Publish the game\n\n";
        out << "Use **Project → Publish Game...** in the Editor and choose an empty "
               "destination folder. The Release build copies the standalone Runtime, "
               "enabled gameplay modules, scene, and Resources into a distributable "
               "folder that does not require the Editor, SDK, CMake, or source tree.\n\n";
        out << "## Debug gameplay code\n\n";
        out << "1. Keep the Frigga Editor open on this project.\n";
        out << "2. Open this folder in VS Code with the Frigga extension.\n";
        out << "3. Run **Frigga: Attach Debugger to Editor** (requires C/C++ extension / GDB).\n";
        out << "4. Set breakpoints in your gameplay sources and hit Play in the Editor.\n";
        return out.str();
    }

    std::string MakeGitignore()
    {
        return "build/\n.frigga/\nCMakeUserPresets.json\n";
    }

    bool CopyModuleHeader(const std::filesystem::path &friggaRoot,
                          const std::filesystem::path &projectRoot)
    {
        const auto src = friggaRoot / "include/Frigga/Module/frigga_module.h";
        const auto dst = projectRoot / "include/frigga_module.h";
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
    const std::filesystem::path &projectRoot, const ProjectDescriptor &descIn)
{
    ProjectManagedWriteResult result;
    ProjectDescriptor desc = descIn;
    FillMissingEnginePaths(desc);
    if(desc.friggaRoot.empty() || desc.friggaBuild.empty())
    {
        result.error = "Engine paths (friggaRoot / friggaBuild) are required";
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(projectRoot / "include", ec);
    std::filesystem::create_directories(projectRoot / ProjectDescriptor::ModulesDirName, ec);
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

    if(!WriteCMakeUserPresets(projectRoot, desc))
    {
        result.error = "Failed to write CMakeUserPresets.json";
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

    if(!CopyModuleHeader(desc.friggaRoot, projectRoot))
    {
        result.error = "Failed to copy frigga_module.h";
        return result;
    }

    if(!WriteTextFile(projectRoot / "include/frigga_user_components.hpp",
                       MakeUserComponentsHeader()))
    {
        result.error = "Failed to write frigga_user_components.hpp";
        return result;
    }

    const auto gameplayRoot = projectRoot / ProjectDescriptor::ModulesDirName / "gameplay";
    if(!WriteTextFile(gameplayRoot / "CMakeLists.txt", MakeGameplayModuleCMake()))
    {
        result.error = "Failed to write modules/gameplay/CMakeLists.txt";
        return result;
    }
    if(!std::filesystem::exists(gameplayRoot / ModuleCatalog::ManifestFileName))
    {
        DiscoveredModule gameplayManifest;
        gameplayManifest.id              = "gameplay";
        gameplayManifest.name            = "Gameplay";
        gameplayManifest.target          = "gameplay";
        gameplayManifest.libraryRelative = desc.moduleLibraryRelative.empty()
                                               ? ProjectDescriptor::DefaultLibraryRelative("gameplay")
                                               : desc.moduleLibraryRelative;
        if(!ModuleCatalog::WriteManifest(gameplayRoot, gameplayManifest))
        {
            result.error = "Failed to write modules/gameplay/module.json";
            return result;
        }
    }

    std::string inputError;
    if(!EnsureDefaultInputJson(projectRoot, inputError))
    {
        result.error = inputError;
        return result;
    }

    std::string resourcesError;
    if(!EnsureProjectResources(projectRoot, resourcesError, desc.friggaRoot))
    {
        result.error = resourcesError;
        return result;
    }

    result.ok = true;
    return result;
}

bool ProjectScaffold::WriteCMakeUserPresets(const std::filesystem::path &projectRoot,
                                            const ProjectDescriptor &desc)
{
    return WriteTextFile(projectRoot / "CMakeUserPresets.json", MakeCMakeUserPresets(desc));
}

bool ProjectScaffold::EnsureDefaultInputJson(const std::filesystem::path &projectRoot,
                                             std::string &error)
{
    const auto path = projectRoot / "input.json";
    if(std::filesystem::exists(path))
    {
        return true;
    }
    if(!fg::SaveInputMapFile(path, fg::MakeDefaultInputMap(), &error))
    {
        if(error.empty())
        {
            error = "Failed to write input.json";
        }
        return false;
    }
    return true;
}

bool ProjectScaffold::EnsureProjectResources(const std::filesystem::path &projectRoot,
                                             std::string &error,
                                             const std::filesystem::path &friggaRoot)
{
    static constexpr std::array<std::string_view, 7> kFolders = {
        "Models", "Textures", "Prefabs", "Fonts", "Audio", "Audio/Banks", "Audio/Clips"};

    const auto destRoot = projectRoot / ProjectDescriptor::ResourcesDirName;
    std::error_code ec;
    std::filesystem::create_directories(destRoot, ec);
    if(ec)
    {
        error = "Failed to create Resources/";
        return false;
    }

    auto copyIfMissing = [&](const std::filesystem::path &source,
                             const std::filesystem::path &destination) {
        if(!std::filesystem::is_regular_file(source, ec) ||
           std::filesystem::exists(destination, ec))
        {
            return;
        }
        std::filesystem::create_directories(destination.parent_path(), ec);
        std::filesystem::copy_file(source, destination,
                                   std::filesystem::copy_options::skip_existing, ec);
    };

    auto copyDirFilesIfMissing = [&](const std::filesystem::path &sourceDir,
                                     const std::filesystem::path &destDir) {
        if(!std::filesystem::is_directory(sourceDir, ec))
        {
            return;
        }
        for(const auto &entry : std::filesystem::directory_iterator(sourceDir, ec))
        {
            if(ec || !entry.is_regular_file(ec))
            {
                continue;
            }
            copyIfMissing(entry.path(), destDir / entry.path().filename());
        }
    };

    std::filesystem::path templateRoot =
        fg::AssetRegistry::EngineResourcesRoot() / "ProjectTemplate";
    if(!std::filesystem::is_directory(templateRoot, ec) && !friggaRoot.empty())
    {
        templateRoot = friggaRoot / "src" / "Editor" / "Resources" / "ProjectTemplate";
    }
    if(!std::filesystem::is_directory(templateRoot, ec))
    {
        templateRoot.clear();
    }

    for(const auto folder : kFolders)
    {
        const auto destDir = destRoot / folder;
        std::filesystem::create_directories(destDir, ec);
        if(ec)
        {
            error = "Failed to create Resources/" + std::string(folder);
            return false;
        }
        if(!templateRoot.empty())
        {
            copyDirFilesIfMissing(templateRoot / folder, destDir);
        }
    }

    const auto engineRoot = fg::AssetRegistry::EngineResourcesRoot();
    copyIfMissing(engineRoot / "Textures" / "default_gray.png",
                  destRoot / "Textures" / "default_gray.png");
    copyIfMissing(engineRoot / "Textures" / "default_roughness.png",
                  destRoot / "Textures" / "default_roughness.png");
    copyIfMissing(engineRoot / "Fonts" / "NotoSans-Regular.ttf",
                  destRoot / "Fonts" / "NotoSans-Regular.ttf");

    return true;
}

bool ProjectScaffold::WriteExampleUserComponents(const std::filesystem::path &projectRoot,
                                                 std::string &error)
{
    // Always refresh the example Health header so migrators pick up fr::Component.
    if(!WriteTextFile(projectRoot / ProjectDescriptor::ModulesDirName / "gameplay" /
                          "src/components/Health.hpp",
                      MakeHealthComponentHpp()))
    {
        error = "Failed to write modules/gameplay/src/components/Health.hpp";
        return false;
    }
    return true;
}

bool ProjectScaffold::MaybeRewriteManagedModuleEntry(const std::filesystem::path &projectRoot,
                                                     std::string &error)
{
    const auto modulePath =
        projectRoot / ProjectDescriptor::ModulesDirName / "gameplay" / "src/GameplayModule.cpp";
    const bool exists     = std::filesystem::exists(modulePath);
    if(exists && !FileContains(modulePath, ManagedModuleMarker))
    {
        return true;
    }
    if(!WriteTextFile(modulePath, MakeGameplayModuleCpp()))
    {
        error = "Failed to write modules/gameplay/src/GameplayModule.cpp";
        return false;
    }
    return true;
}

bool ProjectScaffold::MaybeRewriteManagedGameplaySystem(const std::filesystem::path &projectRoot,
                                                        std::string &error)
{
    const auto hppPath = projectRoot / ProjectDescriptor::ModulesDirName / "gameplay" /
                         "src/systems/GameplaySystem.hpp";
    const auto cppPath = projectRoot / ProjectDescriptor::ModulesDirName / "gameplay" /
                         "src/systems/GameplaySystem.cpp";
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
        error = "Failed to write modules/gameplay/src/systems/GameplaySystem.*";
        return false;
    }
    return true;
}

namespace
{
    std::string ToPascalCase(std::string_view raw)
    {
        std::string out;
        bool upper = true;
        for(const char ch : raw)
        {
            if(!std::isalnum(static_cast<unsigned char>(ch)))
            {
                upper = true;
                continue;
            }
            if(upper)
            {
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                upper = false;
            }
            else
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
        }
        return out.empty() ? "Module" : out;
    }

    std::string MakeExtraModuleCpp(const std::string &)
    {
        return R"cpp(#include <Frigga/Module/FriModule.hpp>

FRI_MODULE(module)
{
}
)cpp";
    }

    std::string MakeExtraModuleCMake(const std::string &target, const std::string &sourceFile)
    {
        std::ostringstream out;
        out << "frigga_add_module(" << target << "\n";
        out << "  src/" << sourceFile << "\n";
        out << ")\n";
        return out.str();
    }

    bool RegisterModuleEntry(ProjectDescriptor &desc, ProjectModuleEntry entry)
    {
        for(auto &existing : desc.modules)
        {
            if(existing.id == entry.id || existing.target == entry.target)
            {
                existing = std::move(entry);
                desc.SyncGameplayMirror();
                return true;
            }
        }
        desc.modules.push_back(std::move(entry));
        desc.SyncGameplayMirror();
        return true;
    }
} // namespace

bool ProjectScaffold::SyncManagedModuleSubdirs(const std::filesystem::path &projectRoot,
                                               const ProjectDescriptor &desc, std::string &error)
{
    const auto cmakePath = projectRoot / "CMakeLists.txt";
    if(!std::filesystem::exists(cmakePath))
    {
        error = "CMakeLists.txt not found";
        return false;
    }
    std::ifstream in(cmakePath, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();
    const auto block = MakeManagedModuleSubdirsBlock(desc);
    const auto begin = text.find(ManagedModuleSubdirsBegin);
    const auto end   = text.find(ManagedModuleSubdirsEnd);
    if(begin != std::string::npos && end != std::string::npos && end > begin)
    {
        const auto endLine = text.find('\n', end);
        const auto replaceUntil = endLine == std::string::npos ? text.size() : endLine + 1;
        text.replace(begin, replaceUntil - begin, block);
    }
    else
    {
        if(!text.empty() && text.back() != '\n')
        {
            text.push_back('\n');
        }
        text += "\n";
        text += block;
    }
    if(!WriteTextFile(cmakePath, text))
    {
        error = "Failed to update CMakeLists.txt module subdirectories";
        return false;
    }
    return true;
}

bool ProjectScaffold::CreateExtraModule(const std::filesystem::path &projectRoot,
                                        ProjectDescriptor &desc, std::string name,
                                        std::string &error)
{
    const auto id = ModuleCatalog::SanitizeId(name);
    const auto moduleRoot = projectRoot / ProjectDescriptor::ModulesDirName / id;
    if(std::filesystem::exists(moduleRoot))
    {
        error = "Module already exists: " + id;
        return false;
    }

    const auto sourceFile = ToPascalCase(name.empty() ? id : name) + "Module.cpp";
    std::error_code ec;
    std::filesystem::create_directories(moduleRoot / "src" / "components", ec);
    std::filesystem::create_directories(moduleRoot / "src" / "systems", ec);
    if(!WriteTextFile(moduleRoot / "src" / sourceFile, MakeExtraModuleCpp(id)) ||
       !WriteTextFile(moduleRoot / "CMakeLists.txt", MakeExtraModuleCMake(id, sourceFile)))
    {
        error = "Failed to write module sources";
        return false;
    }

    DiscoveredModule manifest;
    manifest.id               = id;
    manifest.name             = name.empty() ? id : name;
    manifest.target           = id;
    manifest.libraryRelative  = ProjectDescriptor::DefaultLibraryRelative(id);
    if(!ModuleCatalog::WriteManifest(moduleRoot, manifest))
    {
        error = "Failed to write module.json";
        return false;
    }

    RegisterModuleEntry(desc, ProjectModuleEntry {.id              = id,
                                                  .target          = id,
                                                  .libraryRelative = manifest.libraryRelative,
                                                  .enabled         = true,
                                                  .source          = ModuleSource::Project});
    desc.EnsureGameplayModule();
    if(!SyncManagedModuleSubdirs(projectRoot, desc, error))
    {
        return false;
    }
    return true;
}

bool ProjectScaffold::InstallModule(const std::filesystem::path &projectRoot,
                                    ProjectDescriptor &desc,
                                    const std::filesystem::path &sourceRoot, std::string &error)
{
    auto discovered = ModuleCatalog::ReadManifest(sourceRoot);
    if(!discovered)
    {
        error = "module.json not found in " + sourceRoot.string();
        return false;
    }
    const auto id         = ModuleCatalog::SanitizeId(discovered->id);
    const auto moduleRoot = projectRoot / ProjectDescriptor::ModulesDirName / id;
    if(!ModuleCatalog::CopyModuleTree(sourceRoot, moduleRoot, error))
    {
        return false;
    }
    if(!std::filesystem::exists(moduleRoot / "CMakeLists.txt"))
    {
        const auto sourceFile = ToPascalCase(discovered->name.empty() ? id : discovered->name) + "Module.cpp";
        if(!WriteTextFile(moduleRoot / "CMakeLists.txt", MakeExtraModuleCMake(id, sourceFile)))
        {
            error = "Failed to write installed module CMakeLists.txt";
            return false;
        }
    }

    RegisterModuleEntry(desc, ProjectModuleEntry {.id              = id,
                                                  .target          = discovered->target.empty()
                                                                         ? id
                                                                         : discovered->target,
                                                  .libraryRelative = ProjectDescriptor::DefaultLibraryRelative(
                                                      discovered->target.empty() ? id
                                                                                 : discovered->target),
                                                  .enabled         = true,
                                                  .source          = ModuleSource::User});
    desc.EnsureGameplayModule();
    return SyncManagedModuleSubdirs(projectRoot, desc, error);
}

ProjectScaffoldResult ProjectScaffold::Create(const std::filesystem::path &parentDir,
                                              const ProjectDescriptor &descIn,
                                              fg::Scene &scene)
{
    ProjectScaffoldResult result;
    ProjectDescriptor desc = descIn;
    desc.formatVersion     = ProjectDescriptor::CurrentFormatVersion;
    desc.EnsureGameplayModule();

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
    FillMissingEnginePaths(desc);

    if(desc.moduleLibraryRelative.empty())
    {
        desc.moduleLibraryRelative = DefaultModuleLibraryRelative(desc.moduleTarget);
    }

    const auto projectRoot = parentDir / desc.name;
    std::error_code ec;
    if(std::filesystem::exists(projectRoot))
    {
        result.error = "Project directory already exists: " + projectRoot.string();
        return result;
    }

    std::filesystem::create_directories(projectRoot / "scenes", ec);
    std::filesystem::create_directories(projectRoot / "include", ec);
    std::filesystem::create_directories(projectRoot / "build", ec);
    std::filesystem::create_directories(projectRoot / ProjectDescriptor::ModulesDirName, ec);
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

    if(!MaybeRewriteManagedModuleEntry(projectRoot, result.error) ||
       !MaybeRewriteManagedGameplaySystem(projectRoot, result.error))
    {
        return result;
    }

    const auto gameplayRoot = projectRoot / ProjectDescriptor::ModulesDirName / "gameplay";
    if(!WriteTextFile(gameplayRoot / "CMakeLists.txt", MakeGameplayModuleCMake()))
    {
        result.error = "Failed to write modules/gameplay/CMakeLists.txt";
        return result;
    }
    DiscoveredModule gameplayManifest;
    gameplayManifest.id              = "gameplay";
    gameplayManifest.name            = "Gameplay";
    gameplayManifest.target          = "gameplay";
    gameplayManifest.libraryRelative = desc.moduleLibraryRelative;
    if(!ModuleCatalog::WriteManifest(gameplayRoot, gameplayManifest))
    {
        result.error = "Failed to write modules/gameplay/module.json";
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
