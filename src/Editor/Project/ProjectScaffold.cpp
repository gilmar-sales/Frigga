#include "ProjectScaffold.hpp"

#include "ProjectEnginePaths.hpp"
#include "ProjectFile.hpp"

#include <Frigga/Input/InputMap.hpp>
#include <Frigga/Input/InputMapIO.hpp>

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
        out << "  ${FREYA_INCLUDE}\n";
        out << "  ${GLM_INCLUDE}\n";
        out << "  ${SIMDJSON_INCLUDE}\n";
        out << ")\n\n";
        out << "find_package(Threads REQUIRED)\n";
        out << "# Freyr/Skirnir/Frigga live in the Editor process. Do not link libfreyr.a\n";
        out << "# into this SHARED lib (TLS local-exec / not -fPIC-safe).\n";
        out << "target_link_libraries(" << desc.pluginTarget << " PRIVATE Threads::Threads)\n";
        out << "if(UNIX AND NOT APPLE)\n";
        out << "  target_link_options(" << desc.pluginTarget << " PRIVATE -Wl,--allow-shlib-undefined)\n";
        out << "elseif(WIN32)\n";
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
        out << "  if(NOT _FRIGGA_EDITOR_IMPLIB)\n";
        out << "    message(FATAL_ERROR \"Editor import library not found under ${FRIGGA_BUILD} (or parent). Rebuild the Editor.\")\n";
        out << "  endif()\n";
        out << "  target_link_libraries(" << desc.pluginTarget << " PRIVATE \"${_FRIGGA_EDITOR_IMPLIB}\")\n";
        out << "endif()\n";
        out << "set_target_properties(" << desc.pluginTarget << " PROPERTIES\n";
        out << "  CXX_STANDARD 26\n";
        out << "  CXX_STANDARD_REQUIRED ON\n";
        out << "  CXX_EXTENSIONS ON\n";
        out << "  LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/build\"\n";
        out << "  RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/build\"\n";
        out << "  ARCHIVE_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/build\"\n";
        out << ")\n";
        out << "if(WIN32)\n";
        out << "  # MinGW prefixes shared libs with lib; plugin.library is build/<target>.dll.\n";
        out << "  set_target_properties(" << desc.pluginTarget
           << " PROPERTIES PREFIX \"\" IMPORT_PREFIX \"\")\n";
        out << "endif()\n";
        return out.str();
    }

    std::string MakeGameplaySystemHpp()
    {
        return R"cpp(// FRIGGA_MANAGED_GAMEPLAY_SYSTEM
#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/ThirdPersonCameraComponent.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

/**
 * Freyr system owned by the gameplay plugin.
 * Resolves host fg::Input / fg::Physics via late DI (FRI_PLUGIN_MODULE .System).
 */
class GameplaySystem: public fr::System
{
  public:
    GameplaySystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Input> &input,
                   const skr::Arc<fg::Physics> &physics);

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::Input> mInput;
    skr::Arc<fg::Physics> mPhysics;
};
)cpp";
    }

    std::string MakeGameplaySystemCpp()
    {
        return R"cpp(// FRIGGA_MANAGED_GAMEPLAY_SYSTEM
#include "GameplaySystem.hpp"
#include "Components/Health.hpp"

#include <cmath>

GameplaySystem::GameplaySystem(const skr::Arc<fr::Registry> &registry,
                               const skr::Arc<fg::Input> &input,
                               const skr::Arc<fg::Physics> &physics)
    : fr::System(registry), mInput(input), mPhysics(physics)
{
}

void GameplaySystem::Update(float deltaTime)
{
    // Clamp example Health values.
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

    if(!mInput || !mPhysics)
    {
        return;
    }

    const float horizontal = mInput->GetAxis("Horizontal");
    const float vertical   = mInput->GetAxis("Vertical");
    const bool jump        = mInput->WasPressed("Jump");
    const float speed      = 4.0f;
    const float jumpSpeed  = 5.0f;

    float cameraYaw = 0.0f;
    bool  hasOrbit  = false;
    mRegistry->CreateMutation()->Each<fg::ThirdPersonCameraComponent>(
        [&](fr::Entity, fg::ThirdPersonCameraComponent &orbit) {
            cameraYaw = orbit.yaw;
            hasOrbit  = true;
        });

    mRegistry->CreateMutation()->Each<fg::NameComponent>(
        [&](fr::Entity entity, fg::NameComponent &name) {
            if(name.name != "Player")
            {
                return;
            }

            glm::vec3 desired;
            if(hasOrbit)
            {
                const float yawRad = glm::radians(cameraYaw);
                const glm::vec3 forward {-std::sin(yawRad), 0.0f, -std::cos(yawRad)};
                const glm::vec3 right {std::cos(yawRad), 0.0f, -std::sin(yawRad)};
                desired = (right * horizontal + forward * vertical) * speed;
            }
            else
            {
                desired = {horizontal * speed, 0.0f, -vertical * speed};
            }
            const bool grounded = mPhysics->IsCharacterGrounded(entity);
            if(jump && grounded)
            {
                desired.y = jumpSpeed;
            }
            else if(!grounded)
            {
                // Preserve vertical; world step integrates gravity each tick.
                desired.y = mPhysics->GetCharacterVelocity(entity).y;
            }
            mPhysics->MoveCharacter(entity, desired);
        });
}
)cpp";
    }

    std::string MakeGameplayPluginCpp()
    {
        return R"cpp(// FRIGGA_MANAGED_PLUGIN_ENTRY
#include "GameplaySystem.hpp"
#include "Components/Health.hpp"

#include <Frigga/Plugin/FriPluginModule.hpp>

FRI_PLUGIN_MODULE(plugin)
{
    plugin.Component<Health>()
          .System<GameplaySystem>();
}
)cpp";
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
        out << "- `scenes/main.json` — default scene\n";
        out << "- `src/GameplaySystem.*` — Freyr system (registers via FRI_PLUGIN_MODULE, DI "
               "`fg::Input` + `fg::Physics`)\n";
        out << "- `src/GameplayPlugin.cpp` — FRI_PLUGIN_MODULE entry for the Editor\n";
        out << "- `src/Components/` — project POD components (example: Health)\n";
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
        out << "Edit mode disables Simulation (physics + gameplay); Animation/Render stay on "
               "**Main**.\n\n";
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

    std::string inputError;
    if(!EnsureDefaultInputJson(projectRoot, inputError))
    {
        result.error = inputError;
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
