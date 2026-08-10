#include "EmptyApp.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/ECS/Components/CameraComponent.hpp>
#include <Frigga/ECS/Components/LightComponent.hpp>
#include <Frigga/ECS/Components/MaterialComponent.hpp>
#include <Frigga/ECS/Components/MeshComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/Components/UserDataComponent.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSerializer.hpp>

#include <Freya/Core/Renderer.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Logging/Logger.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr float kEpsilon = 1e-4f;

    [[nodiscard]] std::filesystem::path FixturePath(const char *relative)
    {
#ifdef FRIGGA_TEST_FIXTURES_DIR
        return std::filesystem::path(FRIGGA_TEST_FIXTURES_DIR) / relative;
#else
        return std::filesystem::path("test/fixtures") / relative;
#endif
    }

    void ExpectVec3Near(const glm::vec3 &actual, const glm::vec3 &expected)
    {
        EXPECT_NEAR(actual.x, expected.x, kEpsilon);
        EXPECT_NEAR(actual.y, expected.y, kEpsilon);
        EXPECT_NEAR(actual.z, expected.z, kEpsilon);
    }

    void ExpectQuatNear(const glm::quat &actual, const glm::quat &expected)
    {
        const float dot =
            actual.w * expected.w + actual.x * expected.x + actual.y * expected.y +
            actual.z * expected.z;
        const glm::quat aligned = dot < 0.0f ? -actual : actual;
        EXPECT_NEAR(aligned.w, expected.w, kEpsilon);
        EXPECT_NEAR(aligned.x, expected.x, kEpsilon);
        EXPECT_NEAR(aligned.y, expected.y, kEpsilon);
        EXPECT_NEAR(aligned.z, expected.z, kEpsilon);
    }

    struct EntitySnapshot
    {
        std::string name;
        bool hasTransform = false;
        fg::TransformComponent transform {};
        bool hasMesh = false;
        fg::PrimitiveType primitive = fg::PrimitiveType::Cube;
        bool meshCastShadows = true;
        bool hasCamera = false;
        fg::CameraComponent camera {};
        bool hasLight = false;
        fg::LightComponent light {};
        bool hasRigidBody = false;
        fg::RigidBodyComponent rigidBody {};
    };

    [[nodiscard]] std::vector<EntitySnapshot>
    CaptureEntities(const skr::Arc<fr::Registry> &registry,
                    const skr::Arc<fg::PrimitiveMeshFactory> &primitives)
    {
        registry->ExecuteTasks();

        std::vector<EntitySnapshot> entities;
        registry->CreateMutation()->Each<fg::NameComponent>(
            [&](auto entity, fg::NameComponent &name) {
                EntitySnapshot snap {.name = name.name};

                registry->TryGetComponents<fg::TransformComponent>(
                    entity, [&](fg::TransformComponent &transform) {
                        snap.hasTransform = true;
                        snap.transform    = transform;
                    });

                registry->TryGetComponents<fg::MeshComponent>(entity, [&](fg::MeshComponent &mesh) {
                    snap.hasMesh         = true;
                    snap.meshCastShadows = mesh.castShadows;
                    if(!primitives->TryFindPrimitive(mesh.meshId, snap.primitive))
                    {
                        snap.primitive = fg::PrimitiveType::Cube;
                    }
                });

                registry->TryGetComponents<fg::CameraComponent>(
                    entity, [&](fg::CameraComponent &camera) {
                        snap.hasCamera = true;
                        snap.camera    = camera;
                    });

                registry->TryGetComponents<fg::LightComponent>(entity, [&](fg::LightComponent &light) {
                    snap.hasLight = true;
                    snap.light    = light;
                });

                registry->TryGetComponents<fg::RigidBodyComponent>(
                    entity, [&](fg::RigidBodyComponent &rigidBody) {
                        snap.hasRigidBody = true;
                        snap.rigidBody    = rigidBody;
                        snap.rigidBody.body.Reset();
                    });

                entities.push_back(std::move(snap));
            });

        std::sort(entities.begin(), entities.end(),
                  [](const EntitySnapshot &a, const EntitySnapshot &b) { return a.name < b.name; });
        return entities;
    }

    void ExpectEntitiesEqual(const std::vector<EntitySnapshot> &actual,
                             const std::vector<EntitySnapshot> &expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for(std::size_t i = 0; i < expected.size(); ++i)
        {
            SCOPED_TRACE(expected[i].name);
            EXPECT_EQ(actual[i].name, expected[i].name);
            ASSERT_EQ(actual[i].hasTransform, expected[i].hasTransform);
            if(expected[i].hasTransform)
            {
                ExpectVec3Near(actual[i].transform.position, expected[i].transform.position);
                ExpectVec3Near(actual[i].transform.scale, expected[i].transform.scale);
                ExpectQuatNear(actual[i].transform.rotation, expected[i].transform.rotation);
            }

            ASSERT_EQ(actual[i].hasMesh, expected[i].hasMesh);
            if(expected[i].hasMesh)
            {
                EXPECT_EQ(actual[i].primitive, expected[i].primitive);
                EXPECT_EQ(actual[i].meshCastShadows, expected[i].meshCastShadows);
            }

            ASSERT_EQ(actual[i].hasCamera, expected[i].hasCamera);
            if(expected[i].hasCamera)
            {
                EXPECT_NEAR(actual[i].camera.fovDegrees, expected[i].camera.fovDegrees, kEpsilon);
                EXPECT_NEAR(actual[i].camera.nearPlane, expected[i].camera.nearPlane, kEpsilon);
                EXPECT_NEAR(actual[i].camera.farPlane, expected[i].camera.farPlane, kEpsilon);
                EXPECT_EQ(actual[i].camera.primary, expected[i].camera.primary);
                EXPECT_EQ(actual[i].camera.locked, expected[i].camera.locked);
            }

            ASSERT_EQ(actual[i].hasLight, expected[i].hasLight);
            if(expected[i].hasLight)
            {
                EXPECT_EQ(actual[i].light.type, expected[i].light.type);
                ExpectVec3Near(actual[i].light.color, expected[i].light.color);
                EXPECT_NEAR(actual[i].light.radius, expected[i].light.radius, kEpsilon);
                EXPECT_NEAR(actual[i].light.intensity, expected[i].light.intensity, kEpsilon);
                EXPECT_NEAR(actual[i].light.halfWidth, expected[i].light.halfWidth, kEpsilon);
                EXPECT_NEAR(actual[i].light.halfHeight, expected[i].light.halfHeight, kEpsilon);
                EXPECT_EQ(actual[i].light.castShadows, expected[i].light.castShadows);
            }

            ASSERT_EQ(actual[i].hasRigidBody, expected[i].hasRigidBody);
            if(expected[i].hasRigidBody)
            {
                EXPECT_EQ(actual[i].rigidBody.motion, expected[i].rigidBody.motion);
                EXPECT_EQ(actual[i].rigidBody.shape, expected[i].rigidBody.shape);
                ExpectVec3Near(actual[i].rigidBody.halfExtents, expected[i].rigidBody.halfExtents);
                EXPECT_NEAR(actual[i].rigidBody.radius, expected[i].rigidBody.radius, kEpsilon);
                EXPECT_NEAR(actual[i].rigidBody.height, expected[i].rigidBody.height, kEpsilon);
                EXPECT_NEAR(actual[i].rigidBody.mass, expected[i].rigidBody.mass, kEpsilon);
                EXPECT_NEAR(actual[i].rigidBody.friction, expected[i].rigidBody.friction, kEpsilon);
                EXPECT_NEAR(actual[i].rigidBody.restitution, expected[i].rigidBody.restitution,
                            kEpsilon);
                EXPECT_EQ(actual[i].rigidBody.collisionLayer, expected[i].rigidBody.collisionLayer);
                EXPECT_EQ(actual[i].rigidBody.collideWithLayers,
                          expected[i].rigidBody.collideWithLayers);
            }
        }
    }
} // namespace

class SceneSerializerSpec: public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                       freyr.WithComponent<fg::NameComponent>()
                           .WithComponent<fg::TransformComponent>()
                           .WithComponent<fg::MeshComponent>()
                           .WithComponent<fg::MaterialComponent>()
                           .WithComponent<fg::CameraComponent>()
                           .WithComponent<fg::LightComponent>()
                           .WithComponent<fg::RigidBodyComponent>();
                   })
                   .Build<EmptyApp>();

        mRegistry   = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
        mLogger     = skr::MakeArc<skr::Logger<fg::Scene>>(skr::MakeArc<skr::LoggerOptions>());
        mPrimitives = skr::MakeArc<fg::PrimitiveMeshFactory>(fg::PrimitiveMeshFactory::Catalog);
        mAssets     = skr::MakeArc<fg::AssetRegistry>(fg::AssetRegistry::Catalog);
        mScene      = skr::MakeArc<fg::Scene>(skr::Arc<fra::Renderer> {}, mLogger, mRegistry,
                                              mPrimitives, mAssets);

        ASSERT_EQ(mPrimitives->GetMesh(fg::PrimitiveType::Cube), 1u);
        ASSERT_EQ(mPrimitives->GetDefaultMaterial(), 1u);
    }

    void TearDown() override
    {
        mScene.reset();
        mAssets.reset();
        mPrimitives.reset();
        mLogger.reset();
        mRegistry.reset();
        mApp.reset();
    }

    skr::Arc<EmptyApp> mApp;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<skr::Logger<fg::Scene>> mLogger;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fg::Scene> mScene;
};

TEST_F(SceneSerializerSpec, RoundTrip_DefaultScenePreservesEntities)
{
    const auto before = CaptureEntities(mRegistry, mPrimitives);
    ASSERT_FALSE(before.empty());
    const auto editorBefore = mScene->GetEditorCamera();

    std::string json;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, json));
    ASSERT_FALSE(json.empty());
    ASSERT_TRUE(mScene->RestoreSnapshot(json));

    ExpectEntitiesEqual(CaptureEntities(mRegistry, mPrimitives), before);
    ExpectVec3Near(mScene->GetEditorCamera().transform.position,
                   editorBefore.transform.position);
    ExpectQuatNear(mScene->GetEditorCamera().transform.rotation,
                   editorBefore.transform.rotation);
    EXPECT_NEAR(mScene->GetEditorCamera().fovDegrees, editorBefore.fovDegrees, kEpsilon);
}

TEST_F(SceneSerializerSpec, RoundTrip_DoubleSerializeIsStable)
{
    std::string first;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, first));
    ASSERT_TRUE(mScene->RestoreSnapshot(first));

    std::string second;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, second));
    ASSERT_TRUE(mScene->RestoreSnapshot(second));

    std::string third;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, third));
    EXPECT_EQ(second, third);
}

TEST_F(SceneSerializerSpec, LoadFixture_RichScene)
{
    const auto path = FixturePath("scenes/rich_scene.json");
    ASSERT_TRUE(std::filesystem::exists(path)) << path;
    ASSERT_TRUE(mScene->LoadScene(path));

    const auto entities = CaptureEntities(mRegistry, mPrimitives);
    ASSERT_EQ(entities.size(), 4u);

    std::unordered_map<std::string, const EntitySnapshot *> byName;
    for(const auto &entity : entities)
    {
        byName.emplace(entity.name, &entity);
    }

    ASSERT_TRUE(byName.contains("Ground"));
    ASSERT_TRUE(byName.contains("Dynamic Sphere"));
    ASSERT_TRUE(byName.contains("Main Camera"));
    ASSERT_TRUE(byName.contains("Key Light"));

    const auto &ground = *byName["Ground"];
    ASSERT_TRUE(ground.hasMesh);
    EXPECT_EQ(ground.primitive, fg::PrimitiveType::Cube);
    ASSERT_TRUE(ground.hasRigidBody);
    EXPECT_EQ(ground.rigidBody.motion, fg::BodyMotionType::Static);
    EXPECT_EQ(ground.rigidBody.collisionLayer, 0);
    ExpectVec3Near(ground.transform.position, {0.0f, -0.5f, 0.0f});
    ExpectVec3Near(ground.transform.scale, {4.0f, 1.0f, 4.0f});

    const auto &sphere = *byName["Dynamic Sphere"];
    ASSERT_TRUE(sphere.hasMesh);
    EXPECT_EQ(sphere.primitive, fg::PrimitiveType::Sphere);
    ASSERT_TRUE(sphere.hasRigidBody);
    EXPECT_EQ(sphere.rigidBody.motion, fg::BodyMotionType::Dynamic);
    EXPECT_EQ(sphere.rigidBody.shape, fg::ColliderShape::Sphere);
    EXPECT_NEAR(sphere.rigidBody.mass, 2.5f, kEpsilon);
    ExpectVec3Near(sphere.transform.position, {0.0f, 2.5f, 0.0f});

    const auto &camera = *byName["Main Camera"];
    ASSERT_TRUE(camera.hasCamera);
    EXPECT_TRUE(camera.camera.locked);
    EXPECT_TRUE(camera.camera.primary);

    fr::Entity mainCameraEntity = {};
    mRegistry->CreateMutation()->Each<fg::NameComponent, fg::CameraComponent>(
        [&](auto entity, fg::NameComponent &name, fg::CameraComponent &) {
            if(name.name == "Main Camera")
            {
                mainCameraEntity = entity;
            }
        });
    EXPECT_TRUE(mScene->IsMainCamera(mainCameraEntity));
    EXPECT_EQ(mScene->GetMainCameraEntity(), mainCameraEntity);

    const auto &light = *byName["Key Light"];
    ASSERT_TRUE(light.hasLight);
    EXPECT_EQ(light.light.type, fra::LightType::Point);
    ExpectVec3Near(light.light.color, {1.0f, 0.9f, 0.8f});
    ExpectVec3Near(light.transform.position, {4.0f, 6.0f, 2.0f});
    EXPECT_NEAR(light.light.intensity, 30.0f, kEpsilon);

    EXPECT_NEAR(mScene->GetEditorCamera().fovDegrees, 55.0f, kEpsilon);
    ExpectVec3Near(mScene->GetEditorCamera().transform.position, {5.0f, 4.0f, 6.0f});
}

TEST_F(SceneSerializerSpec, LoadFixture_RoundTrips)
{
    const auto path = FixturePath("scenes/rich_scene.json");
    ASSERT_TRUE(mScene->LoadScene(path));

    const auto before       = CaptureEntities(mRegistry, mPrimitives);
    const auto editorBefore = mScene->GetEditorCamera();

    std::string json;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, json));
    ASSERT_TRUE(mScene->RestoreSnapshot(json));

    ExpectEntitiesEqual(CaptureEntities(mRegistry, mPrimitives), before);
    ExpectVec3Near(mScene->GetEditorCamera().transform.position,
                   editorBefore.transform.position);
    ExpectQuatNear(mScene->GetEditorCamera().transform.rotation,
                   editorBefore.transform.rotation);
    EXPECT_NEAR(mScene->GetEditorCamera().fovDegrees, editorBefore.fovDegrees, kEpsilon);
    EXPECT_NEAR(mScene->GetEditorCamera().nearPlane, editorBefore.nearPlane, kEpsilon);
    EXPECT_NEAR(mScene->GetEditorCamera().farPlane, editorBefore.farPlane, kEpsilon);
}

TEST_F(SceneSerializerSpec, SaveLoad_TempFilePreservesScene)
{
    const auto before = CaptureEntities(mRegistry, mPrimitives);

    const auto path =
        std::filesystem::temp_directory_path() / "frigga_scene_serializer_roundtrip.json";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    ASSERT_TRUE(mScene->SaveScene(path));
    ASSERT_TRUE(std::filesystem::exists(path));
    ASSERT_TRUE(mScene->HasPath());
    EXPECT_EQ(mScene->GetPath(), path);

    mScene->NewScene();
    EXPECT_FALSE(mScene->HasPath());

    ASSERT_TRUE(mScene->LoadScene(path));
    ExpectEntitiesEqual(CaptureEntities(mRegistry, mPrimitives), before);

    std::filesystem::remove(path, ec);
}

TEST_F(SceneSerializerSpec, Deserialize_RejectsInvalidJson)
{
    ASSERT_FALSE(fg::SceneSerializer::Deserialize(*mScene, "{ not json"));
}

TEST_F(SceneSerializerSpec, CaptureRestore_KeepsDisplayPathIntact)
{
    const auto path =
        std::filesystem::temp_directory_path() / "frigga_scene_serializer_path.json";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    ASSERT_TRUE(mScene->SaveScene(path));
    ASSERT_TRUE(mScene->HasPath());

    std::string snapshot;
    ASSERT_TRUE(mScene->CaptureSnapshot(snapshot));
    ASSERT_TRUE(mScene->RestoreSnapshot(snapshot));
    EXPECT_EQ(mScene->GetPath(), path);

    std::filesystem::remove(path, ec);
}

TEST_F(SceneSerializerSpec, RoundTrip_UserComponents)
{
    fg::UserDataComponent data {};
    fg::UserComponentInstance health {.typeId = "Health"};
    health.properties.push_back(fg::NamedProperty {
        .name  = "current",
        .value = fg::PropertyValue {.kind = fg::PropertyKind::Float, .floatValue = 42.5f},
    });
    health.properties.push_back(fg::NamedProperty {
        .name  = "max",
        .value = fg::PropertyValue {.kind = fg::PropertyKind::Float, .floatValue = 100.0f},
    });
    data.instances.push_back(std::move(health));

    mRegistry->CreateEntity(fg::NameComponent {.name = "WithHealth"},
                            fg::TransformComponent {}, std::move(data));
    mRegistry->ExecuteTasks();

    std::string json;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, json));
    EXPECT_NE(json.find("\"userComponents\""), std::string::npos);
    EXPECT_NE(json.find("\"Health\""), std::string::npos);
    ASSERT_TRUE(mScene->RestoreSnapshot(json));

    bool found = false;
    mRegistry->CreateMutation()->Each<fg::NameComponent, fg::UserDataComponent>(
        [&](auto, fg::NameComponent &name, fg::UserDataComponent &userData) {
            if(name.name != "WithHealth")
            {
                return;
            }
            found = true;
            ASSERT_EQ(userData.instances.size(), 1u);
            EXPECT_EQ(userData.instances[0].typeId, "Health");
            ASSERT_EQ(userData.instances[0].properties.size(), 2u);
            const auto *current = fg::FindProperty(userData.instances[0], "current");
            const auto *max     = fg::FindProperty(userData.instances[0], "max");
            ASSERT_NE(current, nullptr);
            ASSERT_NE(max, nullptr);
            EXPECT_EQ(current->value.kind, fg::PropertyKind::Float);
            EXPECT_NEAR(current->value.floatValue, 42.5f, kEpsilon);
            EXPECT_NEAR(max->value.floatValue, 100.0f, kEpsilon);
        });
    EXPECT_TRUE(found);
}

