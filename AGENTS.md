# AGENTS.md

Personal C++26 game engine + ImGui editor. Deeper architecture narrative: `CLAUDE.md`.
When touching Editor UI, also read `src/Editor/AGENTS.md` (font/DPI scale rules).

## Toolchain

- CMake ≥ 3.29, C++26 **with reflection** (GCC 16+ or Clang 22+), Vulkan SDK with `glslc` on PATH.
- `build/` is already configured on this machine: MinGW `C:/mingw64/bin/g++.exe`, Ninja, Debug.
  Reuse it — first configure is slow (FetchContent pulls Freyr/Freya/ImGui/Jolt/… + shader compile).
- All deps come from FetchContent; pinned tags live in root `CMakeLists.txt`.
  README's dep/format version lists lag — trust CMake and
  `include/Frigga/Serialization/FormatVersions.hpp`.

## Build / test / verify

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # only if not configured yet
cmake --build build                    # targets: frigga, Editor, Runtime, frigga_tests
ctest --test-dir build --output-on-failure
ctest --test-dir build -R SceneSerializer --output-on-failure   # one suite
build/frigga_tests --gtest_filter='PrefabSpec.*'                # one test (.exe on Windows)
python3 tools/frigga-mcp/test_server.py                         # MCP bridge tests (stdlib unittest)
python3 tools/docs-mcp/test_docs.py                            # docs MCP tests (stdlib, mocked network)
```

- Before `ctest`, build **all** targets, not just `frigga_tests`: `RuntimeSmoke*` asserts that
  `Runtime[.exe]`, `Resources/**/*.spv`, and `Sdk/FriggaSdkConfig.cmake` exist in the build dir.
- Tests run with the build dir as CWD; fixtures come from `test/fixtures` (`FRIGGA_TEST_FIXTURES_DIR`).
- New tests: `test/*.cpp` specs are listed **explicitly** in `test/CMakeLists.txt` — add yours there
  (no glob).
- Format with `clang-format` (`.clang-format`: Microsoft base, 4-space, 100 cols,
  `SpaceBeforeParens: Never`, namespace contents indented). `.clang-tidy` exists, but CI runs neither
  formatter nor tidy — keep changes formatted yourself.
- CI (`.github/workflows/cmake-multi-platform.yml`): Release build + ctest on Linux (gcc-16) and
  Windows (MinGW gcc 16.2); Linux additionally runs sanitizer, coverage, and benchmark builds; the
  macOS job only validates platform conditionals (`-DFRIGGA_PLATFORM_CONTRACT_ONLY=ON`). Pushes
  ignore `*.md` and `docs/**`.
- Commits: Conventional Commits with scopes (`perf(render+anim): ...`, `fix(game-ui): ...`).

## CMake gotchas

- The `src/Editor` source glob has **no** `CONFIGURE_DEPENDS` (engine/Runtime globs do): after adding
  or deleting Editor `.cpp/.hpp` files, re-run `cmake -S . -B build` or the build won't see them.
- `src/Editor/Resources` is copied into `build/Resources` at **configure** time (`file(COPY)`), so new
  resource files also need a re-configure. Run `Editor` / `Runtime` from the build directory so
  `./Resources` resolves.
- Gameplay modules resolve symbols from the host exe (`ENABLE_EXPORTS`). On Windows the export table
  is a generated `.def` (`cmake/GenerateModuleExports.cmake`): frigga/freyr/skirnir/simdjson export
  unfiltered, but Freya is filtered by a mangled-name allowlist (`_freya_module_filter` in root
  `CMakeLists.txt`) — new Freya APIs called from modules must be added there or linking fails on Windows.
- The gameplay-SDK ABI is checked at configure time and recorded in **three** places — bump all on an
  ABI break: `FRIGGA_SDK_ABI_VERSION` (root `CMakeLists.txt`), `_FRIGGA_SDK_EXPECTED_ABI_VERSION`
  (`cmake/FriggaSdk.cmake`), `FormatVersion::SdkAbi` (`FormatVersions.hpp`).
- README's `cmake --preset publish` is stale: no `CMakePresets.json` exists in the repo. Publish via
  the Editor's **Project → Publish Game...** (it configures `build-release/` itself).

## Architecture (short)

- Targets: `frigga` static engine lib (`src/Frigga` impl + `include/Frigga` public headers, PCH
  `src/Frigga/pch.hpp`), `Editor` (`src/Editor`), `Runtime` (`src/Runtime` — also the published game
  host; its sources are synced into `Sdk/Runtime` post-build, so keep them self-contained).
  Namespaces: `fg` (Frigga), `fr` (Freyr/ECS), `fra` (Freya/render), `skr` (Skirnir/DI).
- Bootstrap: `FriggaExtension` (`src/Frigga/Frigga.cpp`) registers components, pipelines, and DI
  services; systems obtain them by constructor injection.
- ECS pipelines (names in `include/Frigga/ECS/EcsLayout.hpp`, persisted per-project in `ecs.json`):
  `Simulation` (60 Hz, Play only), `Main` (display rate, Play only), `Render` (always ticks last).
- Controllers (`AudioController`, `AnimationController`) only set component intents; the matching
  **system** is the sole owner of the backend (e.g. `AudioSystem` ↔ `IAudioEngine`). Keep that split.
- Format changes: bump `FormatVersions.hpp`, add a migration path (Editor **File → Migrate Project
  Files**), and cover with serializer tests. README's stated format versions can be stale.
- Editor UI: layer stack (`HomeLayer` → `MainLayer`); Workflows (`src/Editor/Workflows`) own dock
  layouts and panels (`src/Editor/Panels`). UI/DPI scale rules: `src/Editor/AGENTS.md`.

## MCP (controlling the Editor)

- `.mcp.json` starts the Python stdio bridge `tools/frigga-mcp/server.py`; the **Editor must be
  running** (the bridge starts lazily and reconnects on the next tool call). Details and tool list:
  `docs/mcp-editor.md`.
- `.mcp.json` also starts `tools/docs-mcp/server.py`, which searches the Freyr/Freya/Skirnir
  markdown docs **from GitHub at the pinned ref** (no `build/_deps` needed). It imports
  `../frigga-mcp/transports.py`, so the two directories must stay siblings (also when packaged
  into `Sdk/tools/`). Version pins resolve from `FRIGGA_SDK_DEPS` → `GIT_TAG` → `main`. Details:
  `docs/mcp-docs.md`.
- `tools/vscode-frigga` is a separate pnpm project: `pnpm compile` (tsc), `pnpm package` (vsce).
