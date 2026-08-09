#include "Scene.hpp"

#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Scene/SceneSerializer.hpp"

#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        TransformComponent makeLookAtTransform(const glm::vec3 &position, const glm::vec3 &target,
                                               const glm::vec3 &worldUp = {0.0f, 1.0f, 0.0f})
        {
            const glm::vec3 forward = glm::normalize(target - position);
            // Match Freya/OpenGL: camera looks along local -Z.
            return TransformComponent {
                .position = position,
                .scale    = {1.0f, 1.0f, 1.0f},
                .rotation = glm::quatLookAt(forward, worldUp),
            };
        }
    } // namespace

    Scene::Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
                 const skr::Arc<fr::Registry> &ecsRegistry,
                 const skr::Arc<PrimitiveMeshFactory> &primitives,
                 const skr::Arc<AssetRegistry> &assets)
        : mEcsRegistry(ecsRegistry), mRenderer(renderer), mLogger(logger), mPrimitives(primitives),
          mAssets(assets)
    {
        CreateDefaultEntities();
    }

    void Scene::Update(float ts)
    {
        mLogger->LogTrace("scene update");
    }

    void Scene::OnEditorRender(float ts) {}

    void Scene::FlushEcs()
    {
        mEcsRegistry->ExecuteTasks();
    }

    void Scene::ClearEntities()
    {
        std::vector<fr::Entity> entities;
        mEcsRegistry->CreateMutation()->Each<NameComponent>(
            [&](auto entity, NameComponent &) { entities.push_back(entity); });

        for(const auto entity : entities)
        {
            mEcsRegistry->DestroyEntity(entity);
        }

        FlushEcs();
        mMainCameraEntity = {};
    }

    void Scene::CreateDefaultEntities()
    {
        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Cube"}, TransformComponent {},
            MeshComponent {.meshId = mPrimitives->GetMesh(PrimitiveType::Cube)},
            MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});

        // Blender-like default framing: elevated 3/4 view looking at the unit cube.
        constexpr glm::vec3 cubeCenter {0.0f, 0.0f, 0.0f};
        constexpr glm::vec3 cameraPosition {4.0f, 3.0f, 4.0f};
        const auto          lookAt = makeLookAtTransform(cameraPosition, cubeCenter);

        mEditorCamera = EditorCamera {
            .transform   = lookAt,
            .fovDegrees  = 50.0f,
            .nearPlane   = 0.1f,
            .farPlane    = 1000.0f,
        };

        mMainCameraEntity = mEcsRegistry->CreateEntity(
            NameComponent {.name = "Main Camera"}, lookAt,
            CameraComponent {.fovDegrees = 50.0f,
                             .nearPlane  = 0.1f,
                             .farPlane   = 1000.0f,
                             .primary    = true,
                             .locked     = true});

        // Default key light: elevated, aiming down at the cube (-Y along local -Z).
        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Point Light"},
            TransformComponent {.position = {4.0f, 6.0f, 2.0f},
                                .scale    = {1.0f, 1.0f, 1.0f},
                                .rotation = glm::quatLookAt(glm::vec3 {0.0f, -1.0f, 0.0f},
                                                            glm::vec3 {0.0f, 0.0f, 1.0f})},
            LightComponent {.type      = fra::LightType::Point,
                            .color     = {1.0f, 1.0f, 1.0f},
                            .radius    = 40.0f,
                            .intensity = 30.0f});

        FlushEcs();
    }

    void Scene::NewScene()
    {
        ClearEntities();
        CreateDefaultEntities();
        mPath.clear();
        PreferEditorCamera();
        ClearRenderIsolation();
        mLogger->LogInformation("Created new scene");
    }

    bool Scene::SaveScene(const std::filesystem::path &path)
    {
        if(!SceneSerializer::Save(*this, path))
        {
            return false;
        }
        mPath = path;
        return true;
    }

    bool Scene::SaveScene()
    {
        if(mPath.empty())
        {
            mLogger->LogWarning("SaveScene called without a path");
            return false;
        }
        return SaveScene(mPath);
    }

    bool Scene::LoadScene(const std::filesystem::path &path)
    {
        ClearEntities();
        if(!SceneSerializer::Load(*this, path))
        {
            CreateDefaultEntities();
            mPath.clear();
            return false;
        }

        FlushEcs();
        mPath = path;
        PreferEditorCamera();
        ClearRenderIsolation();
        return true;
    }

    bool Scene::CaptureSnapshot(std::string &outJson)
    {
        FlushEcs();
        return SceneSerializer::Serialize(*this, outJson);
    }

    bool Scene::RestoreSnapshot(std::string_view json)
    {
        // Clear only after a successful deserialize would lose the live edit scene on
        // parse failure — parse into a temporary Scene first isn't cheap, so: reject
        // empty payloads, then clear and restore; if restore fails recreate defaults.
        if(json.empty())
        {
            return false;
        }

        ClearEntities();
        if(!SceneSerializer::Deserialize(*this, json))
        {
            CreateDefaultEntities();
            return false;
        }

        FlushEcs();
        return true;
    }

} // namespace FRIGGA_NAMESPACE
