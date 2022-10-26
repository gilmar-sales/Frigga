#include "Scene.hpp"

#include <imgui.h>

#include <Renderer/FrameBuffer.hpp>

#include "../BootstrapIconsFont.h"

SceneLayer::SceneLayer(shared<fg::Scene> scene): scene(scene)
{
    fg::FrameBufferSpecification spec = {100, 100, 2, true};
    frame                             = fg::FrameBuffer::create(spec);
    scene_renderer                    = std::make_shared<fg::SceneRenderer>(frame);
}

void SceneLayer::onUpdate()
{
    scene->onEditorRender(scene_renderer, 0.16);
}

void SceneLayer::onGui()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin(ICON_BTSP_CAMERAREELS " Scene");
    ImVec2 avail_size = ImGui::GetContentRegionAvail();
    frame->resize(avail_size.x, avail_size.y);
    frame->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    frame->unbind();
    ImGui::Image((void *)(intptr_t)frame->getColorAttachmentRendererID(), avail_size);
    ImGui::End();
    ImGui::PopStyleVar();
}
