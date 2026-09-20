#include "Frigga/Module/FriFreyaUiKeep.hpp"

#include <Freya/Advanced.hpp>
#include <Freya/Asset/TexturePool.hpp>
#include <Freya/Core/Renderer.hpp>
#include <Freya/Core/UiContext.hpp>
#include <Freya/Core/Window.hpp>

namespace FRIGGA_NAMESPACE
{
    void FriKeepFreyaUiSymbols()
    {
        using Ui = fra::UiContext;
        volatile auto begin          = static_cast<void (Ui::*)(float, glm::uvec2)>(&Ui::Begin);
        volatile auto end            = &Ui::End;
        volatile auto beginAnchor    = &Ui::BeginAnchor;
        volatile auto endAnchor      = &Ui::EndAnchor;
        volatile auto beginPanel     = &Ui::BeginPanel;
        volatile auto endPanel       = &Ui::EndPanel;
        volatile auto beginGrid      = &Ui::BeginGrid;
        volatile auto endGrid        = &Ui::EndGrid;
        volatile auto abilitySlot    = &Ui::AbilitySlot;
        volatile auto progressBar    = &Ui::ProgressBar;
        volatile auto label          = &Ui::Label;
        volatile auto textWrapped    = &Ui::TextWrapped;
        volatile auto beginTooltip   = &Ui::BeginTooltip;
        volatile auto endTooltip     = &Ui::EndTooltip;
        volatile auto getUiContext   = &fra::Renderer::GetUiContext;
        volatile auto getViewport    = &fra::RendererAdvanced::GetViewportImage;
        volatile auto createTex      = &fra::TexturePool::CreateTextureFromMemory;
        volatile auto windowWidth    = &fra::Window::GetWidth;
        volatile auto windowHeight   = &fra::Window::GetHeight;
        (void)begin;
        (void)end;
        (void)beginAnchor;
        (void)endAnchor;
        (void)beginPanel;
        (void)endPanel;
        (void)beginGrid;
        (void)endGrid;
        (void)abilitySlot;
        (void)progressBar;
        (void)label;
        (void)textWrapped;
        (void)beginTooltip;
        (void)endTooltip;
        (void)getUiContext;
        (void)getViewport;
        (void)createTex;
        (void)windowWidth;
        (void)windowHeight;
    }
} // namespace FRIGGA_NAMESPACE
