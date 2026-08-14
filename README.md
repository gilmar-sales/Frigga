# Frigga

Personal C++ game engine and ImGui editor built on [Freyr](https://github.com/gilmar-sales/Freyr) (ECS), [Freya](https://github.com/gilmar-sales/Freya) (Vulkan renderer / windowing), and [Jolt Physics](https://github.com/jrouwe/JoltPhysics).

**Status:** early prototype (v0.2.0). The Gameplay workflow is usable: author scenes of primitives, lights, and cameras; save/load JSON; run a play/edit physics simulation. Animation, Audio, and Shading workflows are dock-layout placeholders. Gameplay code is edited in VS Code (Ctrl+Shift+E) and debugged by attaching GDB to the running Editor.

## Features

- Editor with per-workflow dock layouts (Gameplay, ECS, …)
- ECS components: Name, Transform, Mesh, Material, Camera, Light, RigidBody
- Primitive meshes (cube, sphere, capsule, cylinder, cone, plane, quad)
- Editor viewport with fly / orbit / pan, ImGuizmo, and mouse picking
- Separate editor camera vs gameplay (Main Camera) camera
- JSON scene serialization (v1) with open/save dialogs
- Jolt-backed play mode with transform snapshot/restore
- Persistent preferences in the OS preferred config dir (via SDL `GetPrefPath`)

## Requirements

| Tool | Notes |
|------|--------|
| **CMake** | ≥ 3.29 |
| **C++26 compiler** | Reflection support required — **GCC 16+** or **Clang 22+** |
| **Vulkan SDK** | Headers + loader; `glslc` on `PATH` (Freya compiles shaders) |
| **Git + network** | First configure pulls Freyr, Freya, ImGui, ImGuizmo, Jolt, and Freya’s transitive deps (SDL3, glm, Assimp, Skirnir, simdjson, …) via FetchContent |
| **GPU / drivers** | Vulkan-capable GPU and up-to-date drivers |

On Arch Linux (example):

```bash
sudo pacman -S --needed base-devel cmake git vulkan-devel shaderc
# GCC 16+ typically via gcc from core; ensure g++ --version reports 16+
```

## Build

Configure and build from a dedicated build directory (recommended: Ninja):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Targets:

| Target | Description |
|--------|-------------|
| `frigga` | Engine library |
| `Editor` | Editor executable |
| `frigga_tests` | GoogleTest suite (SceneSerializer round-trips / fixtures) |
| `Shaders` | Freya SPIR-V compile (built as a Freya dependency) |
| `package` | CPack archive (`cpack` / `ninja package`) |

CMake copies `src/Editor/Resources` into the build tree (`build/Resources`). Freya also deposits compiled shaders under that tree. **Run the editor from the build directory** so relative resource paths resolve:

```bash
cd build
./Editor
```

### Tests

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFRIGGA_BUILD_TESTS=ON
cmake --build build --target frigga_tests
cd build && ctest --output-on-failure
# or: ./frigga_tests
```

Disable with `-DFRIGGA_BUILD_TESTS=OFF`.

### Package

After a successful build (so `build/Resources` includes shaders):

```bash
cmake --build build --target package
# → frigga-<version>-<system>-<arch>.tar.gz / .zip under build/
```

The archive installs `Editor` plus `Resources/` suitable for running from the extracted folder (`./Editor` with `./Resources` beside it).

Release build:

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
cd build-release && ./Editor
```

### Configure tips

- First configure can take several minutes (FetchContent + shader compile).
- Prefer an out-of-source `build/` directory; the repo already ignores it via `.gitignore`.
- `CMAKE_EXPORT_COMPILE_COMMANDS` is enabled for IDE / clangd support (`build/compile_commands.json`).

## Editor cheat sheet

| Action | Shortcut |
|--------|----------|
| New scene | Ctrl+N |
| Open scene | Ctrl+O |
| Save | Ctrl+S |
| Save as | Ctrl+Shift+S |
| Build gameplay plugin | Ctrl+B |
| Reload gameplay plugin | Ctrl+R |
| Open project in code editor | Ctrl+Shift+E |
| Play / pause / resume | Ctrl+P |
| Step one physics tick | Ctrl+. |
| Stop play (restore edit scene) | Ctrl+Shift+P |
| Toggle collider overlays | Ctrl+Shift+C |
| Gizmo translate / rotate / scale | W / E / R (editor viewport focused) |
| Frame selection | F (editor viewport) |

The Editor starts on a **home screen**: create a 2D/3D project (scaffolds CMake + Freyr gameplay plugin stubs + scene), open an existing `frigga.project`, or pick a recent project. Opening a project auto-migrates older `frigga.project` formats (rewrites managed `CMakeLists.txt` / plugin header / scaffold `GameplaySystem` when still marked managed). Gameplay CMake does not bake machine paths: the Editor passes `-DFRIGGA_SDK` (packaged `Sdk/` next to the binary, or the engine tree), and CLI builds can use the same flag, the `FRIGGA_SDK` environment variable, or local `CMakeUserPresets.json`. Use **File → Migrate Project Files** to force-refresh managed files, then **Build Gameplay Plugin** (Ctrl+B) and **Reload** (Ctrl+R). Plugins use `FRI_PLUGIN_MODULE` to register components/systems/DI. Host `fg::Input` loads `input.json` Actions/Axes; inject it into Freyr systems. **Play** enables the Freyr **Simulation** pipeline (physics + gameplay); edit mode keeps only the **Main** presentation pipeline (animation + render).

### Debug gameplay (VS Code + GDB)

1. Install the Frigga VS Code extension from `tools/vscode-frigga` (and the Microsoft C/C++ extension).
2. Open the gameplay project in the Frigga Editor (writes `.frigga/editor-session.json` with Editor PID / binary path).
3. Open the same project folder in VS Code (**File → Open in Code Editor** / Ctrl+Shift+E).
4. Run **Frigga: Attach Debugger to Editor**, set breakpoints in gameplay sources, and press Play in the Editor.

Build progress and other background work appear in the Editor bottom status bar (click the mini progress indicator to expand the task list).

Workflows other than **Gameplay** / **ECS** are placeholders. Preferences live under the OS preferred dir (`~/.local/share/Frigga/Editor/preferences.json` on Linux); some graphics options need a restart. New projects default to `~/FriggaProjects`.

Default environment map path in preferences may point at a missing HDR under `Resources/Environments/` — place an HDR there or change the path in Preferences.

## Layout

```
src/
  Frigga/          Engine library (ECS, scene I/O, physics, plugins, GUI, render systems)
  Editor/          Editor app (home, projects, workflows, panels, preferences)
    Resources/     Fonts and default textures (copied into the build dir)
CMakeLists.txt
```

Pinned FetchContent tags (see root `CMakeLists.txt`): Freyr `v0.29.0`, Freya `v0.38.0`, Jolt `v5.3.0`, ImGui `docking` fork.

## License

MIT — see [LICENSE.txt](LICENSE.txt).
