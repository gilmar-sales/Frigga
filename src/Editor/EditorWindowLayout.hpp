#pragma once

#include <Freya/Core/Window.hpp>

namespace EditorWindowLayout
{
    /// Saves current window bounds and maximizes for the editor session.
    void PrepareForEditor(fra::Window &window);

    /// Restores bounds saved by PrepareForEditor (no-op if fullscreen or nothing saved).
    void RestoreForHome(fra::Window &window);
} // namespace EditorWindowLayout
