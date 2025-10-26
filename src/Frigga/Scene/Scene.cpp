#include "Scene.hpp"

#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

namespace FRIGGA_NAMESPACE
{

    Scene::Scene(const Ref<fra::Renderer> &renderer, const Ref<skr::Logger<Scene>> &logger,
                 const Ref<fr::Scene> &ecsScene)
        : mRenderer(renderer), mLogger(logger), mEcsScene(ecsScene)
    {
        const auto camera = mEcsScene->CreateEntity();

        mEcsScene->AddComponent(camera, NameComponent{.name = "Main Camera"});
        mEcsScene->AddComponent(camera, TransformComponent{});
        // m_world.addTag<MainCameraTag>(camera);
    }

    void Scene::Update(float ts)
    {
        mLogger->LogTrace("scene update");
        // m_world.update();
    }

    void Scene::OnEditorRender(float ts) {}

} // namespace FRIGGA_NAMESPACE
