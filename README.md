# Frigga

Personal C++ game engine and ImGui editor built on [Freyr](https://github.com/gilmar-sales/Freyr) (ECS), [Freya](https://github.com/gilmar-sales/Freya) (Vulkan renderer / windowing), and [Jolt Physics](https://github.com/jrouwe/JoltPhysics).

**Status:** early prototype (v0.10.0). The Gameplay workflow is usable: author scenes of primitives, lights, and cameras; save/load JSON; run a play/edit physics simulation. Animation workflow supports clip preview and anim graphs. **Audio workflow** uses [miniaudio](https://github.com/mackron/miniaudio) (JSON event banks, clips, mixer, waveform preview). **Shading workflow** provides material browsing, PBR property editing, and an isolated preview viewport (shader graph remains a placeholder). Gameplay code is edited in VS Code (Ctrl+Shift+E) and debugged by attaching GDB to the running Editor.

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

| Tool               | Notes                                                                                                                                             |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| **CMake**          | ≥ 3.29                                                                                                                                            |
| **C++26 compiler** | Reflection support required — **GCC 16+** or **Clang 22+**                                                                                        |
| **Vulkan SDK**     | Headers + loader; `glslc` on `PATH` (Freya compiles shaders)                                                                                      |
| **Git + network**  | First configure pulls Freyr, Freya, ImGui, ImGuizmo, Jolt, and Freya’s transitive deps (SDL3, glm, Assimp, Skirnir, simdjson, …) via FetchContent |
| **GPU / drivers**  | Vulkan-capable GPU and up-to-date drivers                                                                                                         |

### Audio workflow (miniaudio)

Audio is an ECS domain (like Physics / Animation):

- **Components:** `AudioSourceComponent`, `AudioListenerComponent` (Create / Add Component in Hierarchy).
- **Gameplay API:** inject `fg::AudioController` and call `Play` / `Stop` / `Pause` on entities — it only sets component intents.
- **Runtime:** `AudioSystem` (pipeline **Main**, Play mode) is the sole owner of `IAudioEngine` sync (instances, listener pose, 3D).
- **Edit preview:** `AudioController::PreviewEvent` for tooling; stopped automatically when entering Play.
- Without an active listener, the system falls back to the main camera transform.

Authoring clips / banks:

1. Place `.wav` / `.ogg` clips under `Resources/Audio/Clips/`.
2. Define events in a JSON bank (`.audiobank.json`) under `Resources/Audio/Banks/`:

```json
{
  "events": [
    {
      "path": "event:/SFX/Explosion",
      "clip": "Audio/Clips/explosion.wav",
      "volume": 1.0,
      "bus": "bus:/SFX"
    }
  ]
}
```

1. In the Editor: **Import Bank** (Audio workflow) → assign events on the entity **Audio Source** component (Hierarchy or Audio Inspector).
2. Mixer buses: `bus:/Master`, `bus:/SFX`, `bus:/Music`.

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

| Target         | Description                                               |
| -------------- | --------------------------------------------------------- |
| `frigga`       | Engine library                                            |
| `Editor`       | Editor executable                                         |
| `Runtime`      | Standalone game runtime executable                        |
| `frigga_tests` | GoogleTest suite (SceneSerializer round-trips / fixtures) |
| `Shaders`      | Freya SPIR-V compile (built as a Freya dependency)        |
| `package`      | CPack archive (`cpack` / `ninja package`)                 |

CMake copies `src/Editor/Resources` into the build tree (`build/Resources`). That tree is the **engine** pack (UI fonts, shaders, bundled modules, default textures). Freya also deposits compiled shaders under it. **Run the editor from the build directory** so engine resource paths resolve:

```bash
cd build
./Editor
```

Each gameplay project gets its own `Resources/` (Models, Textures, Prefabs, Fonts), copied from `src/Editor/Resources/ProjectTemplate` when the project is created or opened.

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

### Publish a standalone game

With a project open, use **Project → Publish Game...** and choose an empty
destination folder. The Editor configures a Release build, builds all enabled
gameplay modules, installs the standalone `Runtime`, and copies the project
scene and resources into the destination. The resulting folder contains no
Editor, SDK, CMake files, source tree, or `_deps` directory.

The same operation is available from the command line after configuring the
project:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFRIGGA_SDK=/path/to/Frigga/Sdk \
  -DFRIGGA_RUNTIME=/path/to/Frigga/Runtime
cmake --build build --parallel
cmake --install build --prefix /path/to/published-game
```

Run the published game from its output folder. The Runtime also accepts
`--project <frigga.project>` and `--scene <scene.json>` for diagnostics; by
default it loads `frigga.project` and its configured startup scene.

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

| Action                           | Shortcut                            |
| -------------------------------- | ----------------------------------- |
| New scene                        | Ctrl+N                              |
| Open scene                       | Ctrl+O                              |
| Save                             | Ctrl+S                              |
| Save as                          | Ctrl+Shift+S                        |
| Build modules                    | Ctrl+B                              |
| Reload modules                   | Ctrl+R                              |
| Open project in code editor      | Ctrl+Shift+E                        |
| Play / pause / resume            | Ctrl+P                              |
| Step one physics tick            | Ctrl+.                              |
| Stop play (restore edit scene)   | Ctrl+Shift+P                        |
| Toggle collider overlays         | Ctrl+Shift+C                        |
| Gizmo translate / rotate / scale | W / E / R (editor viewport focused) |
| Frame selection                  | F (editor viewport)                 |

The Editor starts on a **home screen**: create a 2D/3D project (scaffolds CMake + Freyr gameplay module stubs + `Resources/` + scene), open an existing `frigga.project`, or pick a recent project. Opening a project refreshes managed scaffold files when needed (`CMakeLists.txt`, module header, `GameplaySystem` when still marked managed). Gameplay CMake consumes the self-contained SDK (`include/`, dependency headers, and `cmake/FriggaSdk.cmake`) through `-DFRIGGA_SDK` (packaged `Sdk/` next to the binary, or the engine tree). CLI builds can use the same flag, the `FRIGGA_SDK` environment variable, or local `CMakeUserPresets.json`. Use **File → Migrate Project Files** to force-refresh managed files, then **Build Modules** (Ctrl+B) and **Reload** (Ctrl+R). Each project has a `gameplay` module plus optional extras under `modules/` (combat, camera, movement, …). Share extras by exporting to `~/Frigga/Modules`. Modules use `FRI_MODULE` to register components/systems/DI. Host `fg::Input` loads `input.json` Actions/Axes; inject it into Freyr systems. Pipeline layout lives in `ecs.json` (ECS workflow editor). **Play** enables the Freyr **Simulation** pipeline at 60 Hz (physics + gameplay). **Main** runs animation (and optional third-person camera module) every frame; **Render** always ticks last.

### Debug gameplay (VS Code + GDB)

1. Install the Frigga VS Code extension from `tools/vscode-frigga` (and the Microsoft C/C++ extension).
2. Open the gameplay project in the Frigga Editor (writes `.frigga/editor-session.json` with Editor PID / binary path).
3. Open the same project folder in VS Code (**File → Open in Code Editor** / Ctrl+Shift+E).
4. Use the **Frigga → Modules** sidebar to create modules, scaffold components/systems, enable/disable, build, install from the user library, or export.
5. Run **Frigga: Attach Debugger to Editor**, set breakpoints in module sources, and press Play in the Editor.

Build progress and other background work appear in the Editor bottom status bar (click the mini progress indicator to expand the task list).

Workflows other than **Gameplay** / **ECS** are placeholders. Preferences live under the OS preferred dir (`~/.local/share/Frigga/Editor/preferences.json` on Linux); some graphics options need a restart. New projects default to `~/Frigga/Projects`; shared modules live in `~/Frigga/Modules`.

Default environment map path in preferences may point at a missing HDR under `Resources/Environments/` — place an HDR there or change the path in Preferences.

## Layout

```plain
src/
  Frigga/          Engine implementations (ECS, scene I/O, physics, modules, GUI, render systems)
  Editor/          Editor app (home, projects, workflows, panels, preferences)
    Resources/     Engine pack: UI fonts, default textures, bundled modules, ProjectTemplate/
include/
  Frigga/          Public engine headers
CMakeLists.txt
```

Gameplay assets (models, textures, prefabs, fonts) live in each project's `Resources/`, not in the engine tree.

Pinned FetchContent tags (see root `CMakeLists.txt`): Freyr `v0.36.0`, Freya `v0.43.0`, Jolt `v5.3.0`, ImGui `docking` fork.

## License

MIT — see [LICENSE.txt](LICENSE.txt).
