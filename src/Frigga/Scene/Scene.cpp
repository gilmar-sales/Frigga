#include "Scene.hpp"

#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

namespace FRIGGA_NAMESPACE
{

    Scene::Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
                 const skr::Arc<fr::Registry> &ecsRegistry)
        : mRenderer(renderer), mLogger(logger), mEcsRegistry(ecsRegistry)
    {
        mEcsRegistry->CreateEntity(NameComponent {.name = "Main Camera"}, TransformComponent {});
    }

    void Scene::Update(float ts)
    {
        mLogger->LogTrace("scene update");
        // m_world.update();
    }

    void Scene::OnEditorRender(float ts) {}

} // namespace FRIGGA_NAMESPACE
