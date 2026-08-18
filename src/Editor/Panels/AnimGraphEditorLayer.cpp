#include "AnimGraphEditorLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/Animation/AnimGraphDefinition.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"

#include <Freya/Asset/AnimGraphDebug.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    constexpr float kNodeWidth  = 200.0f;
    constexpr float kNodeHeight = 78.0f;
    constexpr float kPinRadius  = 7.0f;
    constexpr float kGrid       = 32.0f;

    ImU32 ColorU32(const ImVec4 &c, float alpha = -1.0f)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4 {c.x, c.y, c.z, alpha < 0.0f ? c.w : alpha});
    }

    float DistSq(const ImVec2 &a, const ImVec2 &b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    float DistToBezier(const ImVec2 &p, const ImVec2 &p0, const ImVec2 &p1, const ImVec2 &p2,
                       const ImVec2 &p3)
    {
        float best = 1.0e12f;
        ImVec2 prev = p0;
        for(int i = 1; i <= 20; ++i)
        {
            const float t  = static_cast<float>(i) / 20.0f;
            const float u  = 1.0f - t;
            const ImVec2 c = {
                u * u * u * p0.x + 3.0f * u * u * t * p1.x + 3.0f * u * t * t * p2.x +
                    t * t * t * p3.x,
                u * u * u * p0.y + 3.0f * u * u * t * p1.y + 3.0f * u * t * t * p2.y +
                    t * t * t * p3.y,
            };
            const float dx = p.x - c.x;
            const float dy = p.y - c.y;
            const float d  = dx * dx + dy * dy;
            best           = std::min(best, d);
            (void)prev;
            prev = c;
        }
        return std::sqrt(best);
    }

    void ClipCombo(const char *label, std::string &value, const fg::ModelAsset *model)
    {
        const char *preview = value.empty() ? "(first clip)" : value.c_str();
        if(!ImGui::BeginCombo(label, preview))
        {
            return;
        }
        if(ImGui::Selectable("(first clip)", value.empty()))
        {
            value.clear();
        }
        if(model != nullptr)
        {
            for(const auto &clip : model->clips)
            {
                const bool selected = value == clip.name;
                if(ImGui::Selectable(clip.name.c_str(), selected))
                {
                    value = clip.name;
                }
            }
        }
        ImGui::EndCombo();
    }
} // namespace

AnimGraphEditorLayer::AnimGraphEditorLayer(skr::Arc<fg::AssetRegistry> assets,
                                           skr::Arc<SelectionContext> selection,
                                           skr::Arc<fr::Registry> registry,
                                           skr::Arc<fg::SceneSimulationState> simulation,
                                           skr::Arc<fg::AnimationController> controller)
    : Layer("Anim Graph"), mAssets(std::move(assets)), mSelection(std::move(selection)),
      mRegistry(std::move(registry)), mSimulation(std::move(simulation)),
      mController(std::move(controller))
{
}

ImVec2 AnimGraphEditorLayer::nodeSize() const
{
    return {kNodeWidth * mZoom, kNodeHeight * mZoom};
}

ImVec2 AnimGraphEditorLayer::worldToScreen(const ImVec2 &origin, float x, float y) const
{
    return {origin.x + mPan.x + x * mZoom, origin.y + mPan.y + y * mZoom};
}

ImVec2 AnimGraphEditorLayer::screenToWorld(const ImVec2 &origin, const ImVec2 &screen) const
{
    return {(screen.x - origin.x - mPan.x) / mZoom, (screen.y - origin.y - mPan.y) / mZoom};
}

ImVec2 AnimGraphEditorLayer::inPinPos(const ImVec2 &origin, const fg::AnimGraphStateDef &state) const
{
    const ImVec2 p = worldToScreen(origin, state.editorX, state.editorY);
    return {p.x, p.y + nodeSize().y * 0.5f};
}

ImVec2 AnimGraphEditorLayer::outPinPos(const ImVec2 &origin,
                                       const fg::AnimGraphStateDef &state) const
{
    const ImVec2 p = worldToScreen(origin, state.editorX, state.editorY);
    return {p.x + nodeSize().x, p.y + nodeSize().y * 0.5f};
}

void AnimGraphEditorLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    const fr::Entity selection = mSelection->Get();
    if(selection != mTrackedEntity)
    {
        mTrackedEntity       = selection;
        mSelectedState       = -1;
        mSelectedTransition  = -1;
        mLinkFrom            = -1;
        mDraggingState       = -1;
        mPanning             = false;
    }

    if(selection == SelectionContext::Invalid)
    {
        ImGui::TextDisabled("Select an entity with an Animator to edit its anim graph.");
        ImGui::End();
        return;
    }

    if(!mRegistry->HasComponent<fg::AnimatorComponent>(selection))
    {
        ImGui::TextWrapped("Selected entity has no Animator component.");
        ImGui::End();
        return;
    }

    mRegistry->TryGetComponents<fg::AnimatorComponent>(
        selection, [&](fg::AnimatorComponent &animator) {
            const fg::ModelAsset *model = nullptr;
            if(!animator.modelSource.empty())
            {
                model = mAssets->FindModel(animator.modelSource);
                if(model == nullptr)
                {
                    (void)mAssets->LoadModel(animator.modelSource);
                    model = mAssets->FindModel(animator.modelSource);
                }
            }

            ImGui::Checkbox("Use Anim Graph", &animator.useAnimGraph);
            ImGui::SameLine();
            ImGui::TextDisabled("States · transitions · params");
            if(model != nullptr && !model->clips.empty() &&
               ImGui::Button("Populate from clips"))
            {
                fg::PopulateAnimGraphFromClips(animator.animGraph, *model);
                animator.useAnimGraph = true;
                mSelectedState        = 0;
                mSelectedTransition   = -1;
            }

            ImGui::Separator();
            const float inspectorWidth = 280.0f;
            const ImVec2 avail         = ImGui::GetContentRegionAvail();
            ImGui::BeginChild("##graphCanvas", ImVec2(std::max(80.0f, avail.x - inspectorWidth - 8.0f),
                                                      avail.y),
                              true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoMove);
            drawCanvas(animator, model);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##graphInspector", ImVec2(inspectorWidth, avail.y), true);
            drawInspector(animator, model);
            ImGui::EndChild();
        });

    ImGui::End();
}

void AnimGraphEditorLayer::drawCanvas(fg::AnimatorComponent &animator, const fg::ModelAsset *model)
{
    fg::LayoutAnimGraphIfNeeded(animator.animGraph);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const ImVec2 canvasMax  = {canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y};
    ImGui::InvisibleButton("##canvasHit", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    const bool canvasHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(canvasMin, canvasMax, true);

    const auto &style = ImGui::GetStyle();
    drawList->AddRectFilled(canvasMin, canvasMax, ColorU32(style.Colors[ImGuiCol_WindowBg]));

    const float grid = kGrid * mZoom;
    if(grid >= 8.0f)
    {
        const ImU32 gridColor = ColorU32(style.Colors[ImGuiCol_Border], 0.25f);
        const float ox        = std::fmod(mPan.x, grid);
        const float oy        = std::fmod(mPan.y, grid);
        for(float x = canvasMin.x + ox; x < canvasMax.x; x += grid)
        {
            drawList->AddLine({x, canvasMin.y}, {x, canvasMax.y}, gridColor);
        }
        for(float y = canvasMin.y + oy; y < canvasMax.y; y += grid)
        {
            drawList->AddLine({canvasMin.x, y}, {canvasMax.x, y}, gridColor);
        }
    }

    std::string currentState;
    std::string nextState;
    if(mController)
    {
        currentState = std::string {mController->GetState(mTrackedEntity)};
        if(const auto *graph = mController->TryGetAnimGraph(mTrackedEntity))
        {
            fra::AnimGraphDebugSnapshot snap {};
            graph->CaptureDebugSnapshot(snap);
            nextState = snap.nextState;
        }
    }

    auto &graph = animator.animGraph;
    for(std::size_t i = 0; i < graph.transitions.size(); ++i)
    {
        const auto &tr = graph.transitions[i];
        const int from = fg::FindAnimGraphStateIndex(graph, tr.from);
        const int to   = fg::FindAnimGraphStateIndex(graph, tr.to);
        if(from < 0 || to < 0)
        {
            continue;
        }
        const ImVec2 p0 = outPinPos(canvasMin, graph.states[static_cast<std::size_t>(from)]);
        const ImVec2 p3 = inPinPos(canvasMin, graph.states[static_cast<std::size_t>(to)]);
        const float dx  = std::max(40.0f * mZoom, std::abs(p3.x - p0.x) * 0.45f);
        const ImVec2 p1 {p0.x + dx, p0.y};
        const ImVec2 p2 {p3.x - dx, p3.y};
        const bool selected = mSelectedTransition == static_cast<int>(i);
        const ImU32 color =
            selected ? IM_COL32(255, 196, 72, 255) : ColorU32(style.Colors[ImGuiCol_PlotLines]);
        drawList->AddBezierCubic(p0, p1, p2, p3, color, selected ? 3.5f : 2.0f);
        const ImVec2 mid {(p0.x + p3.x) * 0.5f, (p0.y + p3.y) * 0.5f};
        drawList->AddCircleFilled(mid, 3.5f * mZoom, color);
    }

    if(mLinkFrom >= 0 && mLinkFrom < static_cast<int>(graph.states.size()))
    {
        const ImVec2 p0 = outPinPos(canvasMin, graph.states[static_cast<std::size_t>(mLinkFrom)]);
        const ImVec2 p3 = ImGui::GetIO().MousePos;
        const float dx  = std::max(40.0f * mZoom, std::abs(p3.x - p0.x) * 0.45f);
        drawList->AddBezierCubic(p0, {p0.x + dx, p0.y}, {p3.x - dx, p3.y}, p3,
                                 IM_COL32(120, 200, 255, 220), 2.0f);
    }

    for(std::size_t i = 0; i < graph.states.size(); ++i)
    {
        drawStateNode(drawList, canvasMin, i, graph.states[i], currentState, nextState,
                      animator.playing);
    }

    drawList->PopClipRect();

    if(canvasHovered)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if(wheel != 0.0f)
        {
            const float prev = mZoom;
            mZoom = std::clamp(mZoom + wheel * 0.1f, 0.35f, 2.0f);
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            mPan.x = mouse.x - canvasMin.x - (mouse.x - canvasMin.x - mPan.x) * (mZoom / prev);
            mPan.y = mouse.y - canvasMin.y - (mouse.y - canvasMin.y - mPan.y) * (mZoom / prev);
        }
    }

    if(canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
    {
        mPanning       = true;
        mDraggingState = -1;
        mLinkFrom      = -1;
    }
    if(!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        mPanning = false;
    }
    if(mPanning || (canvasHovered && ImGui::GetIO().KeyAlt &&
                    ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
    {
        mPan.x += ImGui::GetIO().MouseDelta.x;
        mPan.y += ImGui::GetIO().MouseDelta.y;
    }

    if(!mPanning)
    {
        handleCanvasInput(animator, canvasMin, canvasMin, canvasMax);
    }
    (void)model;
}

void AnimGraphEditorLayer::drawStateNode(ImDrawList *drawList, const ImVec2 &origin,
                                         std::size_t index, fg::AnimGraphStateDef &state,
                                         std::string_view currentState, std::string_view nextState,
                                         bool playing)
{
    const ImVec2 p0   = worldToScreen(origin, state.editorX, state.editorY);
    const ImVec2 size = nodeSize();
    const ImVec2 p1 {p0.x + size.x, p0.y + size.y};
    const auto &style = ImGui::GetStyle();

    ImU32 fill = ColorU32(style.Colors[ImGuiCol_ChildBg]);
    ImU32 border = ColorU32(style.Colors[ImGuiCol_Border]);
    if(static_cast<int>(index) == mSelectedState)
    {
        border = IM_COL32(255, 196, 72, 255);
    }
    if(playing && state.name == currentState)
    {
        fill = IM_COL32(42, 92, 58, 255);
        border = IM_COL32(90, 220, 130, 255);
    }
    else if(playing && state.name == nextState)
    {
        border = IM_COL32(90, 180, 255, 255);
    }

    drawList->AddRectFilled(p0, p1, fill, 8.0f * mZoom);
    drawList->AddRect(p0, p1, border, 8.0f * mZoom, 0, 1.5f * mZoom);

    const ImVec2 in  = inPinPos(origin, state);
    const ImVec2 out = outPinPos(origin, state);
    drawList->AddCircleFilled(in, kPinRadius * mZoom, IM_COL32(90, 180, 255, 255));
    drawList->AddCircleFilled(out, kPinRadius * mZoom, IM_COL32(255, 170, 70, 255));

    char title[128];
    std::snprintf(title, sizeof(title), "%s", state.name.c_str());
    const ImVec2 textPos {p0.x + 14.0f * mZoom, p0.y + 10.0f * mZoom};
    drawList->AddText(textPos, ColorU32(style.Colors[ImGuiCol_Text]), title);

    const char *kindLabel = state.kind == "Blend1D"   ? "Blend1D"
                            : state.kind == "Blend2D" ? "Blend2D"
                                                      : "Clip";
    drawList->AddText({textPos.x, textPos.y + 22.0f * mZoom},
                      ColorU32(style.Colors[ImGuiCol_TextDisabled]), kindLabel);
}

void AnimGraphEditorLayer::handleCanvasInput(fg::AnimatorComponent &animator, const ImVec2 &origin,
                                             const ImVec2 &canvasMin, const ImVec2 &canvasMax)
{
    auto &graph = animator.animGraph;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool hovered =
        mouse.x >= canvasMin.x && mouse.x <= canvasMax.x && mouse.y >= canvasMin.y &&
        mouse.y <= canvasMax.y;
    if(!hovered)
    {
        return;
    }

    const bool playing = mSimulation->IsPlaying();
    const float pinHit = (kPinRadius + 4.0f) * mZoom;
    const auto nodeSz  = nodeSize();

    auto hitPinOut = [&]() -> int {
        for(int i = static_cast<int>(graph.states.size()) - 1; i >= 0; --i)
        {
            if(DistSq(mouse, outPinPos(origin, graph.states[static_cast<std::size_t>(i)])) <=
               pinHit * pinHit)
            {
                return i;
            }
        }
        return -1;
    };
    auto hitPinIn = [&]() -> int {
        for(int i = static_cast<int>(graph.states.size()) - 1; i >= 0; --i)
        {
            if(DistSq(mouse, inPinPos(origin, graph.states[static_cast<std::size_t>(i)])) <=
               pinHit * pinHit)
            {
                return i;
            }
        }
        return -1;
    };
    auto hitNode = [&]() -> int {
        for(int i = static_cast<int>(graph.states.size()) - 1; i >= 0; --i)
        {
            const ImVec2 p = worldToScreen(origin, graph.states[static_cast<std::size_t>(i)].editorX,
                                           graph.states[static_cast<std::size_t>(i)].editorY);
            if(mouse.x >= p.x && mouse.x <= p.x + nodeSz.x && mouse.y >= p.y &&
               mouse.y <= p.y + nodeSz.y)
            {
                return i;
            }
        }
        return -1;
    };

    if(!playing && mLinkFrom >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        const int target = hitPinIn();
        if(target >= 0 && target != mLinkFrom)
        {
            const auto &from = graph.states[static_cast<std::size_t>(mLinkFrom)].name;
            const auto &to   = graph.states[static_cast<std::size_t>(target)].name;
            graph.transitions.push_back(fg::AnimGraphTransitionDef {
                .from = from,
                .to   = to,
            });
            animator.useAnimGraph   = true;
            mSelectedTransition     = static_cast<int>(graph.transitions.size() - 1);
            mSelectedState          = -1;
        }
        mLinkFrom = -1;
    }

    if(!playing && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt)
    {
        const int outPin = hitPinOut();
        if(outPin >= 0)
        {
            mLinkFrom           = outPin;
            mSelectedState      = outPin;
            mSelectedTransition = -1;
        }
        else
        {
            const int node = hitNode();
            if(node >= 0)
            {
                mSelectedState      = node;
                mSelectedTransition = -1;
                mDraggingState      = node;
                const auto &st      = graph.states[static_cast<std::size_t>(node)];
                const ImVec2 p      = worldToScreen(origin, st.editorX, st.editorY);
                mDragOffset         = {mouse.x - p.x, mouse.y - p.y};
            }
            else
            {
                int hitLink = -1;
                for(int i = 0; i < static_cast<int>(graph.transitions.size()); ++i)
                {
                    const auto &tr = graph.transitions[static_cast<std::size_t>(i)];
                    const int from = fg::FindAnimGraphStateIndex(graph, tr.from);
                    const int to   = fg::FindAnimGraphStateIndex(graph, tr.to);
                    if(from < 0 || to < 0)
                    {
                        continue;
                    }
                    const ImVec2 p0 = outPinPos(origin, graph.states[static_cast<std::size_t>(from)]);
                    const ImVec2 p3 = inPinPos(origin, graph.states[static_cast<std::size_t>(to)]);
                    const float dx  = std::max(40.0f * mZoom, std::abs(p3.x - p0.x) * 0.45f);
                    if(DistToBezier(mouse, p0, {p0.x + dx, p0.y}, {p3.x - dx, p3.y}, p3) < 8.0f)
                    {
                        hitLink = i;
                        break;
                    }
                }
                mSelectedTransition = hitLink;
                if(hitLink < 0)
                {
                    mSelectedState = -1;
                }
            }
        }
    }

    if(mDraggingState >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !playing)
    {
        const ImVec2 world = screenToWorld(
            origin, {mouse.x - mDragOffset.x, mouse.y - mDragOffset.y});
        auto &st     = graph.states[static_cast<std::size_t>(mDraggingState)];
        st.editorX   = world.x;
        st.editorY   = world.y;
    }
    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        mDraggingState = -1;
    }

    if(!playing && ImGui::BeginPopupContextWindow("##AnimGraphCtx"))
    {
        const ImVec2 world = screenToWorld(origin, mouse);
        if(ImGui::MenuItem("Add Clip State"))
        {
            fg::AnimGraphStateDef state {};
            state.name    = fg::UniqueAnimGraphStateName(graph, "State");
            state.kind    = "Clip";
            state.editorX = world.x;
            state.editorY = world.y;
            graph.states.push_back(std::move(state));
            if(graph.entry.empty())
            {
                graph.entry = graph.states.back().name;
            }
            animator.useAnimGraph = true;
            mSelectedState        = static_cast<int>(graph.states.size() - 1);
        }
        if(ImGui::MenuItem("Add Blend1D State"))
        {
            fg::AnimGraphStateDef state {};
            state.name       = fg::UniqueAnimGraphStateName(graph, "Locomotion");
            state.kind       = "Blend1D";
            state.blendParam = "Speed";
            state.editorX    = world.x;
            state.editorY    = world.y;
            graph.states.push_back(std::move(state));
            if(std::ranges::none_of(graph.params, [](const fg::AnimGraphParamDef &p) {
                   return p.name == "Speed";
               }))
            {
                graph.params.push_back(fg::AnimGraphParamDef {
                    .name = "Speed", .kind = "Float", .maxValue = 8.0f, .hasRange = true});
            }
            animator.useAnimGraph = true;
            mSelectedState        = static_cast<int>(graph.states.size() - 1);
        }
        if(mSelectedState >= 0 && ImGui::MenuItem("Set as Entry"))
        {
            graph.entry = graph.states[static_cast<std::size_t>(mSelectedState)].name;
        }
        if(mSelectedState >= 0 && ImGui::MenuItem("Delete State"))
        {
            const auto name = graph.states[static_cast<std::size_t>(mSelectedState)].name;
            std::erase_if(graph.transitions, [&](const fg::AnimGraphTransitionDef &tr) {
                return tr.from == name || tr.to == name;
            });
            graph.states.erase(graph.states.begin() + mSelectedState);
            if(graph.entry == name)
            {
                graph.entry = graph.states.empty() ? std::string {} : graph.states.front().name;
            }
            mSelectedState = -1;
        }
        if(mSelectedTransition >= 0 && ImGui::MenuItem("Delete Transition"))
        {
            graph.transitions.erase(graph.transitions.begin() + mSelectedTransition);
            mSelectedTransition = -1;
        }
        ImGui::EndPopup();
    }

    if(!playing && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
       ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        if(mSelectedTransition >= 0 &&
           mSelectedTransition < static_cast<int>(graph.transitions.size()))
        {
            graph.transitions.erase(graph.transitions.begin() + mSelectedTransition);
            mSelectedTransition = -1;
        }
        else if(mSelectedState >= 0 && mSelectedState < static_cast<int>(graph.states.size()))
        {
            const auto name = graph.states[static_cast<std::size_t>(mSelectedState)].name;
            std::erase_if(graph.transitions, [&](const fg::AnimGraphTransitionDef &tr) {
                return tr.from == name || tr.to == name;
            });
            graph.states.erase(graph.states.begin() + mSelectedState);
            if(graph.entry == name)
            {
                graph.entry = graph.states.empty() ? std::string {} : graph.states.front().name;
            }
            mSelectedState = -1;
        }
    }
}

void AnimGraphEditorLayer::drawInspector(fg::AnimatorComponent &animator,
                                         const fg::ModelAsset *model)
{
    auto &graph          = animator.animGraph;
    const bool playing   = mSimulation->IsPlaying();
    ImGui::TextUnformatted("Parameters");
    ImGui::BeginDisabled(playing);
    if(ImGui::Button("Add Float"))
    {
        graph.params.push_back(fg::AnimGraphParamDef {
            .name = "Speed", .kind = "Float", .maxValue = 8.0f, .hasRange = true});
    }
    ImGui::SameLine();
    if(ImGui::Button("Add Bool"))
    {
        graph.params.push_back(fg::AnimGraphParamDef {.name = "Grounded", .kind = "Bool"});
    }
    ImGui::SameLine();
    if(ImGui::Button("Add Trigger"))
    {
        graph.params.push_back(fg::AnimGraphParamDef {.name = "Attack", .kind = "Trigger"});
    }
    ImGui::EndDisabled();

    int removeParam = -1;
    for(int i = 0; i < static_cast<int>(graph.params.size()); ++i)
    {
        auto &param = graph.params[static_cast<std::size_t>(i)];
        ImGui::PushID(i);
        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", param.name.c_str());
        ImGui::BeginDisabled(playing);
        if(ImGui::InputText("##pname", nameBuf, sizeof(nameBuf)))
        {
            param.name = nameBuf;
        }
        ImGui::EndDisabled();

        if(param.kind == "Float")
        {
            float value = mController->GetFloat(mTrackedEntity, param.name, param.defaultFloat);
            if(ImGui::SliderFloat("##pval", &value, param.hasRange ? param.minValue : 0.0f,
                                  param.hasRange ? param.maxValue : 8.0f))
            {
                mController->SetFloat(mTrackedEntity, param.name, value);
            }
        }
        else if(param.kind == "Bool")
        {
            bool value = mController->GetBool(mTrackedEntity, param.name, param.defaultBool);
            if(ImGui::Checkbox("##pbool", &value))
            {
                mController->SetBool(mTrackedEntity, param.name, value);
            }
        }
        else if(ImGui::Button("Fire"))
        {
            mController->SetTrigger(mTrackedEntity, param.name);
        }

        ImGui::SameLine();
        if(!playing && ImGui::SmallButton("X"))
        {
            removeParam = i;
        }
        ImGui::PopID();
    }
    if(removeParam >= 0)
    {
        graph.params.erase(graph.params.begin() + removeParam);
    }

    ImGui::Separator();
    if(mSelectedTransition >= 0 &&
       mSelectedTransition < static_cast<int>(graph.transitions.size()))
    {
        auto &tr = graph.transitions[static_cast<std::size_t>(mSelectedTransition)];
        ImGui::Text("Transition");
        ImGui::TextDisabled("%s → %s", tr.from.c_str(), tr.to.c_str());
        ImGui::BeginDisabled(playing);
        const char *kinds[] = {"FloatGreater", "FloatLessEqual", "BoolTrue", "BoolFalse",
                               "Trigger"};
        int kindIndex       = 4;
        for(int i = 0; i < 5; ++i)
        {
            if(tr.conditionKind == kinds[i])
            {
                kindIndex = i;
            }
        }
        if(ImGui::Combo("Condition", &kindIndex, kinds, 5))
        {
            tr.conditionKind = kinds[kindIndex];
        }
        char paramBuf[64];
        std::snprintf(paramBuf, sizeof(paramBuf), "%s", tr.param.c_str());
        if(ImGui::BeginCombo("Param", tr.param.empty() ? "(select)" : tr.param.c_str()))
        {
            for(const auto &param : graph.params)
            {
                const bool selected = tr.param == param.name;
                if(ImGui::Selectable(param.name.c_str(), selected))
                {
                    tr.param = param.name;
                }
            }
            ImGui::EndCombo();
        }
        if(tr.conditionKind == "FloatGreater" || tr.conditionKind == "FloatLessEqual")
        {
            ImGui::DragFloat("Threshold", &tr.threshold, 0.01f);
        }
        ImGui::DragFloat("Blend (s)", &tr.blendDuration, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::EndDisabled();
        return;
    }

    if(mSelectedState < 0 || mSelectedState >= static_cast<int>(graph.states.size()))
    {
        ImGui::TextDisabled("Select a state or drag a pin to create a transition.");
        ImGui::TextDisabled("Right-click the canvas to add states.");
        if(!graph.entry.empty())
        {
            ImGui::Text("Entry: %s", graph.entry.c_str());
        }
        return;
    }

    auto &state = graph.states[static_cast<std::size_t>(mSelectedState)];
    ImGui::TextUnformatted("State");
    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", state.name.c_str());
    ImGui::BeginDisabled(playing);
    if(ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
    {
        fg::RenameAnimGraphState(graph, state.name, nameBuf);
    }
    const char *kinds[] = {"Clip", "Blend1D", "Blend2D"};
    int kindIndex       = state.kind == "Blend1D" ? 1 : state.kind == "Blend2D" ? 2 : 0;
    if(ImGui::Combo("Kind", &kindIndex, kinds, 3))
    {
        state.kind = kinds[kindIndex];
    }
    if(state.kind == "Clip")
    {
        ClipCombo("Clip", state.clip, model);
        ImGui::Checkbox("Loop", &state.loop);
        ImGui::DragFloat("Speed", &state.playbackSpeed, 0.01f, 0.0f, 8.0f, "%.2f");
    }
    else
    {
        if(ImGui::BeginCombo("Param X",
                             state.blendParam.empty() ? "(select)" : state.blendParam.c_str()))
        {
            for(const auto &param : graph.params)
            {
                if(param.kind != "Float")
                {
                    continue;
                }
                const bool selected = state.blendParam == param.name;
                if(ImGui::Selectable(param.name.c_str(), selected))
                {
                    state.blendParam = param.name;
                }
            }
            ImGui::EndCombo();
        }
        if(state.kind == "Blend2D")
        {
            if(ImGui::BeginCombo("Param Y", state.blendParamY.empty() ? "(select)"
                                                                     : state.blendParamY.c_str()))
            {
                for(const auto &param : graph.params)
                {
                    if(param.kind != "Float")
                    {
                        continue;
                    }
                    const bool selected = state.blendParamY == param.name;
                    if(ImGui::Selectable(param.name.c_str(), selected))
                    {
                        state.blendParamY = param.name;
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::Checkbox("Sync Phase", &state.syncPhase);
        if(ImGui::Button("Add Sample") && model != nullptr && !model->clips.empty())
        {
            fg::AnimGraphBlendSampleDef sample {};
            sample.clip  = model->clips.front().name;
            sample.value = static_cast<float>(state.blendSamples.size());
            state.blendSamples.push_back(std::move(sample));
        }
        int removeSample = -1;
        for(int i = 0; i < static_cast<int>(state.blendSamples.size()); ++i)
        {
            auto &sample = state.blendSamples[static_cast<std::size_t>(i)];
            ImGui::PushID(i);
            if(state.kind == "Blend2D")
            {
                ImGui::DragFloat2("Pos", &sample.x, 0.01f);
            }
            else
            {
                ImGui::DragFloat("Value", &sample.value, 0.01f);
            }
            ClipCombo("Clip", sample.clip, model);
            if(ImGui::SmallButton("Remove"))
            {
                removeSample = i;
            }
            ImGui::PopID();
        }
        if(removeSample >= 0)
        {
            state.blendSamples.erase(state.blendSamples.begin() + removeSample);
        }
    }

    const bool isEntry = graph.entry == state.name;
    bool entryToggle   = isEntry;
    if(ImGui::Checkbox("Entry State", &entryToggle) && entryToggle)
    {
        graph.entry = state.name;
    }
    ImGui::EndDisabled();
}
