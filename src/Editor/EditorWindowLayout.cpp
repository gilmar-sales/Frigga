#include "EditorWindowLayout.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace
{
    struct SavedBounds
    {
        int x;
        int y;
        int width;
        int height;
    };

    std::optional<SavedBounds> gSavedBounds;
} // namespace

namespace EditorWindowLayout
{
    void PrepareForEditor(fra::Window &window)
    {
        if(window.IsFullscreen())
        {
            return;
        }

        auto *sdlWindow = static_cast<SDL_Window *>(window.NativeWindow());
        if(sdlWindow == nullptr)
        {
            return;
        }

        SavedBounds bounds {};
        SDL_GetWindowPosition(sdlWindow, &bounds.x, &bounds.y);
        SDL_GetWindowSize(sdlWindow, &bounds.width, &bounds.height);
        gSavedBounds = bounds;
        SDL_MaximizeWindow(sdlWindow);
    }

    void RestoreForHome(fra::Window &window)
    {
        if(window.IsFullscreen() || !gSavedBounds)
        {
            gSavedBounds.reset();
            return;
        }

        auto *sdlWindow = static_cast<SDL_Window *>(window.NativeWindow());
        if(sdlWindow == nullptr)
        {
            gSavedBounds.reset();
            return;
        }

        const SavedBounds bounds = *gSavedBounds;
        gSavedBounds.reset();

        SDL_RestoreWindow(sdlWindow);
        SDL_SetWindowSize(sdlWindow, bounds.width, bounds.height);
        SDL_SetWindowPosition(sdlWindow, bounds.x, bounds.y);
    }
} // namespace EditorWindowLayout
