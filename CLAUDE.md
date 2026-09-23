# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Frigga is a personal C++26 game engine + ImGui editor built on the author's own libraries:
**Freyr** (ECS, namespace `fr`), **Freya** (Vulkan renderer/windowing/UI, namespace `fra`),
**Skirnir** (DI / application builder, namespace `skr`), plus Jolt Physics and miniaudio.
Frigga's own namespace is `fg` (via the `FRIGGA_NAMESPACE` macro in `include/Frigga/Macro.hpp`).
All deps come from FetchContent; pinned tags live in the root `CMakeLists.txt` (the README's
version list can lag behind it — trust CMake).

## Build & test

Requires CMake ≥ 3.29, a C++26 compiler with reflection (**GCC 16+** or Clang 22+), and the
Vulkan SDK with `glslc` on `PATH`. The local Windows tree in `build/` is configured with MinGW
GCC (`C:/mingw64/bin/g++.exe`), Ninja, Debug.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # first configure is slow (FetchContent + shaders)
cmake --build build                                      # targets: frigga, Editor, Runtime, frigga_tests
cmake --build build --target frigga_tests
ctest --test-dir build --output-on-failure
ctest --test-dir build -R SceneSerializer --output-on-failure   # single test / suite (gtest_discover_tests)
build/frigga_tests --gtest_filter='PrefabSpec.*'                # or run the binary directly
```

- Tests are GoogleTest `*Spec.cpp` files in `test/`, **listed explicitly** in
  `test/CMakeLists.txt` — add new specs there. Fixtures: `test/fixtures` (`FRIGGA_TEST_FIXTURES_DIR`).
  Tests run with the build dir as working directory.
- Options: `FRIGGA_BUILD_TESTS` (default ON), `FRIGGA_ENABLE_SANITIZERS`, `FRIGGA_ENABLE_COVERAGE`,
  `FRIGGA_BUILD_BENCHMARKS` (target `frigga_benchmarks`, source in `bench/`).
- Run `Editor` / `Runtime` **from the build directory**: CMake copies `src/Editor/Resources` to
  `build/Resources` (engine pack: fonts, shaders, bundled modules, `ProjectTemplate/`).
- Formatting: `.clang-format` (Microsoft base, 4-space, 100 cols, `SpaceBeforeParens: Never`,
  namespace contents indented). `.clang-tidy` is present.
- MCP bridge (Python): `tools/frigga-mcp/server.py`, tests in `tools/frigga-mcp/test_server.py`.
- Docs MCP (Python): `tools/docs-mcp/server.py`, tests in `tools/docs-mcp/test_docs.py`. It reads
  Freyr/Freya/Skirnir `docs/*.md` from GitHub at the pinned ref — no Editor, no configured build —
  reusing `tools/frigga-mcp/transports.py` (keep the directories siblings, also under `Sdk/tools/`).
  See `docs/mcp-docs.md`.
- Commits follow Conventional Commits with scopes, e.g. `perf(render+anim): ...`, `fix(game-ui): ...`.

## Architecture

**Three executables/libs.** `frigga` (static engine lib from `src/Frigga` + `include/Frigga`,
PCH `src/Frigga/pch.hpp`), `Editor` (`src/Editor`), and `Runtime` (`src/Runtime`, standalone
game host). Both executables set `ENABLE_EXPORTS` so dynamically loaded gameplay modules can
resolve Frigga symbols from the host (`cmake/GenerateModuleExports.cmake` produces the `.def`).

**Application bootstrap.** `FriggaExtension` (`src/Frigga/Frigga.cpp`) is a Skirnir extension
that registers all built-in ECS components with Freyr, defines the pipelines, configures Freya,
and registers engine services as DI singletons (`Scene`, `AssetRegistry`, `IPhysicsWorld` →
`JoltPhysicsWorld`, `IAudioEngine` → `MiniaudioEngine`, `Input`, `GameplayModuleHost`, …).
Systems and gameplay code obtain these by constructor injection.

**Pipelines** (names in `include/Frigga/ECS/EcsLayout.hpp`; user layout persisted in a
project's `ecs.json`):
- `Simulation` — 60 Hz fixed rate, Play mode only (physics + gameplay systems).
- `Main` — display rate, Play mode only (physics interpolation, audio, camera modules).
- `Render` — always ticks, last. Holds `AnimationSystem` so Edit mode can preview clips.
Engine systems live in `src/Frigga/ECS/Systems/`; heavy systems iterate with `ForEachChunkAsync`
and must keep GPU uploads thread-safe (e.g. `EnsureSkeletonResident`).

**Controller vs. system split.** Gameplay-facing APIs like `AudioController` /
`AnimationController` only set component intents; the corresponding system is the sole owner
of the backend (e.g. `AudioSystem` is the only code syncing `IAudioEngine`). Keep that
ownership boundary.

**Gameplay modules.** User projects are built against a self-contained SDK
(`include/` + dep headers + `cmake/FriggaSdk.cmake`, packed into `build/Sdk` by
`cmake/PackFriggaSdk.cmake`). Modules register via `FRI_MODULE` (`include/Frigga/Module/FriModule.hpp`)
with fluent `.Component<T>()`, `.System<T>()`, `.Singleton<T>()`; components without a custom
inspector draw callback get a reflection-based inspector (`UserComponentReflection.hpp`, C++26
reflection). `GameplayModuleHost` loads/unloads modules (hot reload) and `FriModuleRuntime`
records detach ops. SDK ABI version is `FRIGGA_SDK_ABI_VERSION`; bump it on ABI breaks.
The `Runtime` post-build step syncs `src/Runtime` into `Sdk/Runtime`, which is what
"Publish Game" compiles — keep Runtime sources self-contained.

**Versioned formats.** `frigga.project` and scene/prefab JSON versions are in
`include/Frigga/Serialization/FormatVersions.hpp`; format changes need a version bump plus a
migration path (Editor "File → Migrate Project Files") and serializer tests.

**Editor.** `EditorApplication` → layer stack (`HomeLayer` for project selection, `MainLayer`
once a project is open). UI is organized into **Workflows** (`src/Editor/Workflows`: Gameplay,
ECS, Animation, Audio, Shading), each with its own dock layout and panels (`src/Editor/Panels`).
Edit vs Play: entering Play snapshots transforms and enables Simulation/Main; Stop restores.
The Editor also hosts a local MCP service (`src/Editor/Mcp`) reached through the Python stdio
bridge in `tools/frigga-mcp` (configured in `.mcp.json`; tools are the `mcp__frigga-editor__*`
set). The Editor must be running for those tools to work. See `docs/mcp-editor.md`.

`.mcp.json` also starts `tools/docs-mcp`, a second stdio server that needs neither the Editor nor a
local checkout: it lists and reads the dependency documentation on GitHub at the ref resolved from
`FRIGGA_SDK_DEPS` (SDK config) → `GIT_TAG` → `main`, caching tree listings per ref and blobs by
SHA. Its tools are the `docs.*` set (`docs.catalog`, `docs.search`, `docs.read`, `docs.refresh`),
each hit carrying a `lib@ref:path:line` citation. See `docs/mcp-docs.md`.

### Editor UI scale rule (from `src/Editor/AGENTS.md`)

Never set `ImGuiIO::FontGlobalScale` directly — use `EditorUiScale::Sync(window->GetScale())`
from `src/Editor/UiScale.hpp` (called every frame in `EditorApplication::Update`). Use
`EditorUiScale::S` / `V` for layout spacing in Home/dialogs. Viewport render-target pixel
scaling is separate (`ViewportDpi.hpp`) and must not be mixed with font scale.
