#include "EditorApplication.hpp"

#include "EditorViewportHost.hpp"
#include "UiScale.hpp"
#include "BoostrapIconsFont.hpp"
#include "EditorTheme.hpp"

EditorApplication::EditorApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider)
    : AbstractApplication(serviceProvider), mRegistry(serviceProvider->GetService<fr::Registry>()),
      mSystemManager(serviceProvider->GetService<fr::SystemManager>()),
      mSimulation(serviceProvider->GetService<fg::SceneSimulationState>()),
      mInput(serviceProvider->GetService<fg::Input>()),
      mMcp(serviceProvider->GetService<ProjectSession>(), serviceProvider->GetService<fg::Scene>(),
           mSimulation, serviceProvider->GetService<skr::Logger<EditorMcpService>>())
{
    PushLayer(mScope->GetServiceProvider()->GetService<HomeLayer>());
    PushLayer(mScope->GetServiceProvider()->GetService<MainLayer>());
    PushLayer(mScope->GetServiceProvider()->GetService<PreferencesLayer>());
    PushLayer(mScope->GetServiceProvider()->GetService<InputMapLayer>());

    EditorUiScale::Sync(mWindow->GetScale());

    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("Resources/Fonts/OpenSans.ttf", 18);

    static const ImWchar icons_ranges[] = {ICON_MIN_BTSP, ICON_MAX_BTSP, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode  = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("Resources/Fonts/BootstrapIconsFont.ttf", 16, &icons_config,
                                 icons_ranges);

    // Edit mode at startup: only Render (animation preview + draw) ticks.
    syncPlayPipelines();
    std::string mcpError;
    if(!mMcp.Start(mcpError))
    {
        std::fprintf(stderr, "Unable to start Editor MCP service: %s\n", mcpError.c_str());
    }
}

void EditorApplication::syncPlayPipelines()
{
    if(!mSystemManager || !mSimulation)
    {
        return;
    }

    const bool playing = mSimulation->IsPlaying();
    if(const auto simId = mSystemManager->FindPipelineId("Simulation"))
    {
        mSystemManager->SetPipelineEnabled(*simId, playing);
    }
    if(const auto mainId = mSystemManager->FindPipelineId("Main"))
    {
        mSystemManager->SetPipelineEnabled(*mainId, playing);
    }
}

void EditorApplication::RenderScene()
{
    // Freyr defers CreateEntity/AddComponents into archetype task queues.
    // Those only drain via ExecuteTasks (or EnqueueTask while the pool is
    // already running). Update() alone runs systems but does not StartTasks on
    // archetypes — so Name/Transform/Camera data would stay defaulted.
    if(mSimulation)
    {
        mSimulation->FlushPending();
    }
    syncPlayPipelines();
    if(mInput)
    {
        mInput->BeginFrame(mWindow->GetDeltaTime());
    }
    mRegistry->ExecuteTasks();
    mRegistry->Update(mWindow->GetDeltaTime());
}

void EditorApplication::Update()
{
    mMcp.Poll();
    // Poll every frame: monitor moves often skip DISPLAY_SCALE_CHANGED until resize.
    EditorUiScale::Sync(mWindow->GetScale());

    EditorViewportHost::BeginFrame();

    if(mSimulation)
    {
        mSimulation->SetDeferModeChanges(true);
    }
    AbstractApplication::Update();
    if(mSimulation)
    {
        mSimulation->SetDeferModeChanges(false);
    }
    // Flush entities created from Hierarchy/menus during onGui this frame.
    mRegistry->ExecuteTasks();
}

void EditorApplication::OnAfterGuiLayout()
{
    EditorViewportHost::ApplyClaims();
}

bool EditorApplication::ShouldBootstrapViewportFallback() const
{
    return !EditorViewportHost::HasActiveClaim();
}
