# Editor agent notes

## UI scale / multi-monitor DPI

Central helper: `UiScale.hpp` (`EditorUiScale`).

- **Never** set `ImGuiIO::FontGlobalScale` ad hoc. Call `EditorUiScale::Sync(window->GetScale())`.
- Sync runs every frame in `EditorApplication::Update` because `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` alone often does **not** fire when dragging between monitors; scale only updated after resize otherwise.
- Event handlers may also call `Sync` via `EditorUiScale::IsDisplayTopologyEvent`.
- For ImGui layout spacing/sizes that should track window work-area (Home, dialogs), use `EditorUiScale::S` / `V` — do not hardcode large paddings without them.
- Viewport render targets still use `EditorViewport` in `ViewportDpi.hpp` (`DisplayFramebufferScale` → pixels). Do not mix that with `FontGlobalScale`.
- Fonts stay authored at base point sizes; display density is applied through `Sync`, not by reloading atlases per monitor move.

### Checklist

- [ ] Moved window 4K@200% ↔ FHD@100% without resizing → fonts/layout update immediately
- [ ] New Home/dialog spacing goes through `EditorUiScale::S`/`V`
- [ ] No second copy of display-scale → `FontGlobalScale` logic outside `UiScale.hpp`
