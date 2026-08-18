#include "ProjectScaffold.hpp"

#include "PluginCatalog.hpp"
#include "ProjectEnginePaths.hpp"
#include "ProjectFile.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Input/InputMap.hpp>
#include <Frigga/Input/InputMapIO.hpp>

#include <array>
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
        const auto root  = EffectiveFriggaRoot(desc).generic_string();
        const auto build = EffectiveFriggaBuild(desc).generic_string();
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
        out << "        \"FRIGGA_ROOT\": \"" << EscapeJson(root) << "\",\n";
        out << "        \"FRIGGA_BUILD\": \"" << EscapeJson(build) << "\"\n";
        out << "      }\n";
        out << "    }\n";
        out << "  ]\n";
        out << "}\n";
        return out.str();
    }

    std::string MakeManagedPluginSubdirsBlock(const ProjectDescriptor &desc)
    {
        std::ostringstream out;
        out << ProjectScaffold::ManagedPluginSubdirsBegin << "\n";
        for(const auto &entry : desc.plugins)
        {
            const auto folder = entry.id.empty() ? entry.target : entry.id;
            out << "if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/"
                << ProjectDescriptor::PluginsDirName << "/" << folder
                << "/CMakeLists.txt\")\n";
            out << "  add_subdirectory(" << ProjectDescriptor::PluginsDirName << "/" << folder
                << ")\n";
            out << "endif()\n";
        }
        out << ProjectScaffold::ManagedPluginSubdirsEnd << "\n";
        return out.str();
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
        out << "set(CMAKE_POSITION_INDEPENDENT_CODE ON)\n";
        out << "# Header reflection only — do not inject -fmodules-ts (CMP0155).\n";
        out << "set(CMAKE_CXX_SCAN_FOR_MODULES 0)\n\n";
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
        out << "# SDK path is never baked in: -DFRIGGA_SDK=, ENV{FRIGGA_SDK}, or CMakeUserPresets.\n";
        out << "set(FRIGGA_SDK \"\" CACHE PATH \"Packaged Frigga Sdk (or engine source tree)\")\n";
        out << "set(FRIGGA_ROOT \"\" CACHE PATH \"Frigga headers root (defaults to FRIGGA_SDK)\")\n";
        out << "set(FRIGGA_BUILD \"\" CACHE PATH "
               "\"Editor binary dir with _deps (defaults to FRIGGA_SDK when packaged)\")\n";
        out << "if(NOT FRIGGA_SDK AND DEFINED ENV{FRIGGA_SDK} AND NOT \"$ENV{FRIGGA_SDK}\" STREQUAL \"\")\n";
        out << "  set(FRIGGA_SDK \"$ENV{FRIGGA_SDK}\" CACHE PATH "
               "\"Packaged Frigga Sdk (or engine source tree)\" FORCE)\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_ROOT AND DEFINED ENV{FRIGGA_ROOT} AND NOT \"$ENV{FRIGGA_ROOT}\" STREQUAL \"\")\n";
        out << "  set(FRIGGA_ROOT \"$ENV{FRIGGA_ROOT}\" CACHE PATH "
               "\"Frigga headers root (defaults to FRIGGA_SDK)\" FORCE)\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_BUILD AND DEFINED ENV{FRIGGA_BUILD} AND NOT \"$ENV{FRIGGA_BUILD}\" STREQUAL \"\")\n";
        out << "  set(FRIGGA_BUILD \"$ENV{FRIGGA_BUILD}\" CACHE PATH "
               "\"Editor binary dir with _deps (defaults to FRIGGA_SDK when packaged)\" FORCE)\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_SDK AND FRIGGA_ROOT)\n";
        out << "  set(FRIGGA_SDK \"${FRIGGA_ROOT}\")\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_ROOT AND FRIGGA_SDK)\n";
        out << "  set(FRIGGA_ROOT \"${FRIGGA_SDK}\")\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_ROOT)\n";
        out << "  message(FATAL_ERROR \"Frigga SDK not found. Configure with "
               "-DFRIGGA_SDK=<Sdk-or-source>, set FRIGGA_SDK, or use CMakeUserPresets.json "
               "from the Editor.\")\n";
        out << "endif()\n";
        out << "if(NOT FRIGGA_BUILD)\n";
        out << "  if(EXISTS \"${FRIGGA_ROOT}/_deps/freyr-src/include/Freyr\")\n";
        out << "    set(FRIGGA_BUILD \"${FRIGGA_ROOT}\")\n";
        out << "  elseif(EXISTS \"${FRIGGA_SDK}/_deps/freyr-src/include/Freyr\")\n";
        out << "    set(FRIGGA_BUILD \"${FRIGGA_SDK}\")\n";
        out << "  else()\n";
        out << "    message(FATAL_ERROR \"FRIGGA_BUILD is required for a Frigga source tree "
               "(Editor binary dir containing _deps/). Pass -DFRIGGA_BUILD=... or set "
               "FRIGGA_BUILD.\")\n";
        out << "  endif()\n";
        out << "endif()\n";
        out << "if(NOT EXISTS \"${FRIGGA_ROOT}/src/Frigga/Plugin/frigga_plugin.h\")\n";
        out << "  message(FATAL_ERROR \"FRIGGA_ROOT does not look like a Frigga tree: "
               "${FRIGGA_ROOT}\")\n";
        out << "endif()\n";
        out << "message(STATUS \"Frigga SDK:   ${FRIGGA_SDK}\")\n";
        out << "message(STATUS \"Frigga root:  ${FRIGGA_ROOT}\")\n";
        out << "message(STATUS \"Frigga build: ${FRIGGA_BUILD}\")\n\n";
        out << "set(FREYR_INCLUDE \"${FRIGGA_BUILD}/_deps/freyr-src/include\")\n";
        out << "set(SKIRNIR_INCLUDE \"${FRIGGA_BUILD}/_deps/skirnir-src/include\")\n";
        out << "set(FREYA_INCLUDE \"${FRIGGA_BUILD}/_deps/freya-src/include\")\n";
        out << "set(GLM_INCLUDE \"${FRIGGA_BUILD}/_deps/glm-src\")\n";
        out << "set(SIMDJSON_INCLUDE \"${FRIGGA_BUILD}/_deps/simdjson-src/include\")\n";
        out << "set(FREYR_LIB_DIR \"${FRIGGA_BUILD}/_deps/freyr-build\")\n";
        out << "set(SKIRNIR_LIB_DIR \"${FRIGGA_BUILD}/_deps/skirnir-build\")\n\n";
        out << "find_package(Threads REQUIRED)\n";
        out << "if(WIN32)\n";
        out << "  set(_FRIGGA_EDITOR_IMPLIB \"\")\n";
        out << "  get_filename_component(_FRIGGA_BUILD_PARENT \"${FRIGGA_BUILD}\" DIRECTORY)\n";
        out << "  get_filename_component(_FRIGGA_SDK_PARENT \"${FRIGGA_SDK}\" DIRECTORY)\n";
        out << "  foreach(_cand IN ITEMS\n";
        out << "      \"${FRIGGA_BUILD}/libEditor.dll.a\"\n";
        out << "      \"${FRIGGA_BUILD}/Editor.lib\"\n";
        out << "      \"${FRIGGA_BUILD}/libEditor.lib\"\n";
        out << "      \"${FRIGGA_SDK}/libEditor.dll.a\"\n";
        out << "      \"${FRIGGA_SDK}/Editor.lib\"\n";
        out << "      \"${_FRIGGA_BUILD_PARENT}/libEditor.dll.a\"\n";
        out << "      \"${_FRIGGA_BUILD_PARENT}/Editor.lib\"\n";
        out << "      \"${_FRIGGA_SDK_PARENT}/libEditor.dll.a\"\n";
        out << "      \"${_FRIGGA_SDK_PARENT}/Editor.lib\")\n";
        out << "    if(EXISTS \"${_cand}\")\n";
        out << "      set(_FRIGGA_EDITOR_IMPLIB \"${_cand}\")\n";
        out << "      break()\n";
        out << "    endif()\n";
        out << "  endforeach()\n";
        out << "endif()\n\n";
        out << "function(frigga_add_plugin TARGET)\n";
        out << "  add_library(${TARGET} SHARED ${ARGN})\n";
        out << "  target_compile_features(${TARGET} PRIVATE cxx_std_26)\n";
        out << "  target_compile_definitions(${TARGET} PRIVATE FRI_PLUGIN_EXPORTS)\n";
        out << "  if(MSVC)\n";
        out << "    target_compile_options(${TARGET} PRIVATE /std:c++latest /experimental:reflection)\n";
        out << "  else()\n";
        out << "    target_compile_options(${TARGET} PRIVATE -std=gnu++26 -freflection)\n";
        out << "  endif()\n";
        out << "  # Same std prelude Freya uses in its library PCH so Event.hpp compiles.\n";
        out << "  target_precompile_headers(${TARGET} PRIVATE\n";
        out << "    <cstdint>\n";
        out << "    <type_traits>\n";
        out << "    \"${FRIGGA_ROOT}/src/Frigga/Plugin/FriPluginSdk.hpp\"\n";
        out << "  )\n";
        out << "  target_include_directories(${TARGET} PRIVATE\n";
        out << "    ${CMAKE_CURRENT_SOURCE_DIR}/include\n";
        out << "    ${CMAKE_CURRENT_SOURCE_DIR}/src\n";
        out << "    ${CMAKE_SOURCE_DIR}/include\n";
        out << "    ${CMAKE_SOURCE_DIR}/src\n";
        out << "    ${FRIGGA_ROOT}/src\n";
        out << "    ${FREYR_INCLUDE}\n";
        out << "    ${SKIRNIR_INCLUDE}\n";
        out << "    ${FREYA_INCLUDE}\n";
        out << "    ${GLM_INCLUDE}\n";
        out << "    ${SIMDJSON_INCLUDE}\n";
        out << "  )\n";
        out << "  target_link_libraries(${TARGET} PRIVATE Threads::Threads)\n";
        out << "  if(UNIX AND NOT APPLE)\n";
        out << "    target_link_options(${TARGET} PRIVATE -Wl,--allow-shlib-undefined)\n";
        out << "  elseif(WIN32)\n";
        out << "    if(NOT _FRIGGA_EDITOR_IMPLIB)\n";
        out << "      message(FATAL_ERROR \"Editor import library not found under ${FRIGGA_BUILD} (or parent). Rebuild the Editor.\")\n";
        out << "    endif()\n";
        out << "    target_link_libraries(${TARGET} PRIVATE \"${_FRIGGA_EDITOR_IMPLIB}\")\n";
        out << "  endif()\n";
        out << "  set_target_properties(${TARGET} PROPERTIES\n";
        out << "    CXX_STANDARD 26\n";
        out << "    CXX_STANDARD_REQUIRED ON\n";
        out << "    CXX_EXTENSIONS ON\n";
        out << "    LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_SOURCE_DIR}/build\"\n";
        out << "    RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_SOURCE_DIR}/build\"\n";
        out << "    ARCHIVE_OUTPUT_DIRECTORY \"${CMAKE_SOURCE_DIR}/build\"\n";
        out << "  )\n";
        out << "  if(WIN32)\n";
        out << "    set_target_properties(${TARGET} PROPERTIES PREFIX \"\" IMPORT_PREFIX \"\")\n";
        out << "  endif()\n";
        out << "endfunction()\n\n";
        out << MakeManagedPluginSubdirsBlock(desc);
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
 * Freyr system owned by the gameplay plugin.
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
#include "systems/GameplaySystem.hpp"
#include "components/Health.hpp"

#include <Frigga/Plugin/FriPluginModule.hpp>

FRI_PLUGIN_MODULE(plugin)
{
    plugin.Component<Health>()
          .System<GameplaySystem>();
}
)cpp";
    }

    std::string MakeGameplayPluginCMake()
    {
        return "frigga_add_plugin(gameplay\n"
               "  src/GameplayPlugin.cpp\n"
               "  src/systems/GameplaySystem.cpp\n"
               ")\n";
    }

    std::string MakeUserComponentsHeader()
    {
        return R"cpp(#pragma once

/**
 * Convenience aliases for project Freyr gameplay components.
 * Register types with FRI_PLUGIN_MODULE: plugin.Component<T>().
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
        out << "- `input.json` — named Actions / Axes bindings\n";
        out << "- `ecs.json` — ECS pipeline / system layout (created on first Editor open)\n";
        out << "- `scenes/main.json` — default scene\n";
        out << "- `Resources/` — models, textures, prefabs, and fonts owned by this project\n";
        out << "- `plugins/` — shared libraries (`gameplay`, optional extras)\n";
        out << "- `plugins/gameplay/src/systems/` — Freyr systems\n";
        out << "- `plugins/gameplay/src/components/` — project POD components (example: Health)\n";
        out << "- `plugins/gameplay/src/GameplayPlugin.cpp` — FRI_PLUGIN_MODULE entry\n";
        out << "- `include/frigga_user_components.hpp` — FriSet / FriTryGet helpers\n\n";
        out << "## Project components\n\n";
        out << "1. Declare `struct Foo : fr::Component { float x; };`\n";
        out << "2. In `FRI_PLUGIN_MODULE`: `plugin.Component<Foo>()`\n";
        out << "3. Build + **Reload Gameplay Plugin**.\n";
        out << "4. In the Editor: Entity → Add Component → Gameplay → Foo.\n";
        out << "5. In a Freyr `System::Update`: `CreateMutation()->Each<Foo>(...)` "
               "(Simulation pipeline — Play mode only).\n\n";
        out << "## Gameplay systems\n\n";
        out << "Inherit `fr::System` and register with `plugin.System<MySystem>()` "
               "(defaults to the **Simulation** pipeline).\n";
        out << "Optional DI: `plugin.Singleton<T>()`, `.Scoped<T>()`, `.Transient<T>()`.\n";
        out << "Host exposes `fg::Input` and `fg::Physics` — inject them in system ctors "
               "(`IsDown`/`WasPressed`/`GetAxis`, `MoveCharacter`/`SetLinearVelocity`).\n";
        out << "Host placement: new plugin systems append to **Simulation** (60 Hz, Play only); "
               "known labels are restored from `ecs.json` after attach. Edit pipelines in the "
               "**ECS** workflow.\n";
        out << "Tick order: **Simulation** (gameplay + physics) → **Main** (camera) → "
               "**Render** (animation preview + draw, always last). Edit mode keeps only "
               "Render; Simulation and Main tick in Play.\n\n";
        out << "## Build the plugin\n\n";
        out << "Requires a **C++26** compiler with reflection (GCC 16+ or Clang 22+), "
               "same as Frigga.\n\n";
        out << "Point CMake at the packaged `Sdk/` next to the Editor (or the engine tree):\n\n";
        out << "```bash\n";
        out << "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug "
               "-DFRIGGA_SDK=/path/to/Sdk\n";
        out << "cmake --build build\n";
        out << "```\n\n";
        out << "Alternatively set the `FRIGGA_SDK` environment variable, or use "
               "`cmake --preset default` after the Editor has written local "
               "`CMakeUserPresets.json` (gitignored).\n";
        out << "Engine developers can also pass `-DFRIGGA_ROOT=` (source) and "
               "`-DFRIGGA_BUILD=` (Editor binary dir with `_deps/`).\n\n";
        out << "Or use **File → Build Gameplay Plugin** (Ctrl+B) in the Editor "
               "(passes SDK paths and forces `gnu++26` + `-freflection`).\n\n";
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
        return "build/\n.frigga/\nCMakeUserPresets.json\n";
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
    std::filesystem::create_directories(projectRoot / ProjectDescriptor::PluginsDirName, ec);
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

    const auto gameplayRoot = projectRoot / ProjectDescriptor::PluginsDirName / "gameplay";
    if(!WriteTextFile(gameplayRoot / "CMakeLists.txt", MakeGameplayPluginCMake()))
    {
        result.error = "Failed to write plugins/gameplay/CMakeLists.txt";
        return result;
    }
    if(!std::filesystem::exists(gameplayRoot / PluginCatalog::ManifestFileName))
    {
        DiscoveredPlugin gameplayManifest;
        gameplayManifest.id              = "gameplay";
        gameplayManifest.name            = "Gameplay";
        gameplayManifest.target          = "gameplay";
        gameplayManifest.libraryRelative = desc.pluginLibraryRelative.empty()
                                               ? ProjectDescriptor::DefaultLibraryRelative("gameplay")
                                               : desc.pluginLibraryRelative;
        if(!PluginCatalog::WriteManifest(gameplayRoot, gameplayManifest))
        {
            result.error = "Failed to write plugins/gameplay/plugin.json";
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
    static constexpr std::array<std::string_view, 4> kFolders = {
        "Models", "Textures", "Prefabs", "Fonts"};

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
    if(!WriteTextFile(projectRoot / ProjectDescriptor::PluginsDirName / "gameplay" /
                          "src/components/Health.hpp",
                      MakeHealthComponentHpp()))
    {
        error = "Failed to write plugins/gameplay/src/components/Health.hpp";
        return false;
    }
    return true;
}

bool ProjectScaffold::MaybeRewriteManagedPluginEntry(const std::filesystem::path &projectRoot,
                                                     std::string &error)
{
    const auto pluginPath =
        projectRoot / ProjectDescriptor::PluginsDirName / "gameplay" / "src/GameplayPlugin.cpp";
    const bool exists     = std::filesystem::exists(pluginPath);
    if(exists && !FileContains(pluginPath, ManagedPluginMarker))
    {
        return true;
    }
    if(!WriteTextFile(pluginPath, MakeGameplayPluginCpp()))
    {
        error = "Failed to write plugins/gameplay/src/GameplayPlugin.cpp";
        return false;
    }
    return true;
}

bool ProjectScaffold::MaybeRewriteManagedGameplaySystem(const std::filesystem::path &projectRoot,
                                                        std::string &error)
{
    const auto hppPath = projectRoot / ProjectDescriptor::PluginsDirName / "gameplay" /
                         "src/systems/GameplaySystem.hpp";
    const auto cppPath = projectRoot / ProjectDescriptor::PluginsDirName / "gameplay" /
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
        error = "Failed to write plugins/gameplay/src/systems/GameplaySystem.*";
        return false;
    }
    return true;
}

namespace
{
    std::string MakeExtraPluginCpp(const std::string &classHint)
    {
        (void)classHint;
        return R"cpp(#include <Frigga/Plugin/FriPluginModule.hpp>

FRI_PLUGIN_MODULE(plugin)
{
}
)cpp";
    }

    std::string MakeExtraPluginCMake(const std::string &target, const std::string &sourceFile)
    {
        std::ostringstream out;
        out << "frigga_add_plugin(" << target << "\n";
        out << "  src/" << sourceFile << "\n";
        out << ")\n";
        return out.str();
    }

    bool RegisterPluginEntry(ProjectDescriptor &desc, ProjectPluginEntry entry)
    {
        for(auto &existing : desc.plugins)
        {
            if(existing.id == entry.id || existing.target == entry.target)
            {
                existing = std::move(entry);
                desc.SyncGameplayMirror();
                return true;
            }
        }
        desc.plugins.push_back(std::move(entry));
        desc.SyncGameplayMirror();
        return true;
    }
} // namespace

bool ProjectScaffold::SyncManagedPluginSubdirs(const std::filesystem::path &projectRoot,
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
    const auto block = MakeManagedPluginSubdirsBlock(desc);
    const auto begin = text.find(ManagedPluginSubdirsBegin);
    const auto end   = text.find(ManagedPluginSubdirsEnd);
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
        error = "Failed to update CMakeLists.txt plugin subdirectories";
        return false;
    }
    return true;
}

bool ProjectScaffold::CreateExtraPlugin(const std::filesystem::path &projectRoot,
                                        ProjectDescriptor &desc, std::string name,
                                        std::string &error)
{
    const auto id = PluginCatalog::SanitizeId(name);
    const auto pluginRoot = projectRoot / ProjectDescriptor::PluginsDirName / id;
    if(std::filesystem::exists(pluginRoot))
    {
        error = "Plugin already exists: " + id;
        return false;
    }

    const auto sourceFile = id + "Plugin.cpp";
    std::error_code ec;
    std::filesystem::create_directories(pluginRoot / "src" / "components", ec);
    std::filesystem::create_directories(pluginRoot / "src" / "systems", ec);
    if(!WriteTextFile(pluginRoot / "src" / sourceFile, MakeExtraPluginCpp(id)) ||
       !WriteTextFile(pluginRoot / "CMakeLists.txt", MakeExtraPluginCMake(id, sourceFile)))
    {
        error = "Failed to write plugin sources";
        return false;
    }

    DiscoveredPlugin manifest;
    manifest.id               = id;
    manifest.name             = name.empty() ? id : name;
    manifest.target           = id;
    manifest.libraryRelative  = ProjectDescriptor::DefaultLibraryRelative(id);
    if(!PluginCatalog::WriteManifest(pluginRoot, manifest))
    {
        error = "Failed to write plugin.json";
        return false;
    }

    RegisterPluginEntry(desc, ProjectPluginEntry {.id              = id,
                                                  .target          = id,
                                                  .libraryRelative = manifest.libraryRelative,
                                                  .enabled         = true,
                                                  .source          = PluginSource::Project});
    desc.EnsureGameplayPlugin();
    if(!SyncManagedPluginSubdirs(projectRoot, desc, error))
    {
        return false;
    }
    return true;
}

bool ProjectScaffold::InstallPlugin(const std::filesystem::path &projectRoot,
                                    ProjectDescriptor &desc,
                                    const std::filesystem::path &sourceRoot, std::string &error)
{
    auto discovered = PluginCatalog::ReadManifest(sourceRoot);
    if(!discovered)
    {
        error = "plugin.json not found in " + sourceRoot.string();
        return false;
    }
    const auto id         = PluginCatalog::SanitizeId(discovered->id);
    const auto pluginRoot = projectRoot / ProjectDescriptor::PluginsDirName / id;
    if(!PluginCatalog::CopyPluginTree(sourceRoot, pluginRoot, error))
    {
        return false;
    }
    if(!std::filesystem::exists(pluginRoot / "CMakeLists.txt"))
    {
        const auto sourceFile = id + "Plugin.cpp";
        if(!WriteTextFile(pluginRoot / "CMakeLists.txt", MakeExtraPluginCMake(id, sourceFile)))
        {
            error = "Failed to write installed plugin CMakeLists.txt";
            return false;
        }
    }

    RegisterPluginEntry(desc, ProjectPluginEntry {.id              = id,
                                                  .target          = discovered->target.empty()
                                                                         ? id
                                                                         : discovered->target,
                                                  .libraryRelative = ProjectDescriptor::DefaultLibraryRelative(
                                                      discovered->target.empty() ? id
                                                                                 : discovered->target),
                                                  .enabled         = true,
                                                  .source          = PluginSource::User});
    desc.EnsureGameplayPlugin();
    return SyncManagedPluginSubdirs(projectRoot, desc, error);
}

ProjectScaffoldResult ProjectScaffold::Create(const std::filesystem::path &parentDir,
                                              const ProjectDescriptor &descIn,
                                              fg::Scene &scene)
{
    ProjectScaffoldResult result;
    ProjectDescriptor desc = descIn;
    desc.formatVersion     = ProjectDescriptor::CurrentFormatVersion;
    desc.EnsureGameplayPlugin();

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

    std::filesystem::create_directories(projectRoot / "scenes", ec);
    std::filesystem::create_directories(projectRoot / "include", ec);
    std::filesystem::create_directories(projectRoot / "build", ec);
    std::filesystem::create_directories(projectRoot / ProjectDescriptor::PluginsDirName, ec);
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

    if(!MaybeRewriteManagedPluginEntry(projectRoot, result.error) ||
       !MaybeRewriteManagedGameplaySystem(projectRoot, result.error))
    {
        return result;
    }

    const auto gameplayRoot = projectRoot / ProjectDescriptor::PluginsDirName / "gameplay";
    if(!WriteTextFile(gameplayRoot / "CMakeLists.txt", MakeGameplayPluginCMake()))
    {
        result.error = "Failed to write plugins/gameplay/CMakeLists.txt";
        return result;
    }
    DiscoveredPlugin gameplayManifest;
    gameplayManifest.id              = "gameplay";
    gameplayManifest.name            = "Gameplay";
    gameplayManifest.target          = "gameplay";
    gameplayManifest.libraryRelative = desc.pluginLibraryRelative;
    if(!PluginCatalog::WriteManifest(gameplayRoot, gameplayManifest))
    {
        result.error = "Failed to write plugins/gameplay/plugin.json";
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
