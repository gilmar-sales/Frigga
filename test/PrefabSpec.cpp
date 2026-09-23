#include "EmptyApp.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/ECS/Components/LightComponent.hpp>
#include <Frigga/ECS/Components/MaterialComponent.hpp>
#include <Frigga/ECS/Components/MeshComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/PrefabComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>
#include <Frigga/ECS/UserComponentRegistry.hpp>
#include <Frigga/Scene/Prefab.hpp>
#include <Frigga/Scene/PrefabCache.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSerializer.hpp>

#include <Freya/Core/Renderer.hpp>
#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

class PrefabSpec: public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                       freyr.WithComponent<fg::NameComponent>()
                           .WithComponent<fg::HierarchyComponent>()
                           .WithComponent<fg::TransformComponent>()
                           .WithComponent<fg::MeshComponent>()
                           .WithComponent<fg::MaterialComponent>()
                           .WithComponent<fg::LightComponent>()
                           .WithComponent<fg::PrefabComponent>();
                   })
                   .Build<EmptyApp>();

        mRegistry   = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
        mLogger     = skr::MakeArc<skr::Logger<fg::Scene>>(skr::MakeArc<skr::LoggerOptions>());
        mPrimitives = skr::MakeArc<fg::PrimitiveMeshFactory>(fg::PrimitiveMeshFactory::Catalog);
        mAssets     = skr::MakeArc<fg::AssetRegistry>(fg::AssetRegistry::Catalog);
        mUserComponents = skr::MakeArc<fg::UserComponentRegistry>();
        mScene      = skr::MakeArc<fg::Scene>(skr::Arc<fra::Renderer> {}, mLogger, mRegistry,
                                              mPrimitives, mAssets, mUserComponents);
    }

    void TearDown() override
    {
        mScene.reset();
        mUserComponents.reset();
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
    skr::Arc<fg::UserComponentRegistry> mUserComponents;
    skr::Arc<fg::Scene> mScene;
};

TEST_F(PrefabSpec, RoundTrip_ParentChildPreservesLocalTransforms)
{
    const auto parent = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Enemy"},
        fg::TransformComponent {.position = {10.0f, 0.0f, 0.0f}},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Cube)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    const auto child = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Weapon"},
        fg::TransformComponent {.position = {1.0f, 2.0f, 3.0f}},
        fg::LightComponent {.type = fra::LightType::Point, .intensity = 12.0f});
    mRegistry->ExecuteTasks();
    ASSERT_TRUE(fg::TransformUtil::Reparent(*mRegistry, child, parent, false));

    std::string json;
    ASSERT_TRUE(fg::Prefab::Serialize(*mScene, parent, json));
    EXPECT_NE(json.find("\"Enemy\""), std::string::npos);
    EXPECT_EQ(json.find("editorCamera"), std::string::npos);

    fr::Entity instance = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Instantiate(*mScene, json, fr::NullEntity, instance));
    ASSERT_NE(instance, fr::NullEntity);

    std::string instanceName;
    mRegistry->TryGetComponents<fg::NameComponent>(
        instance, [&](fg::NameComponent &name) { instanceName = name.name; });
    EXPECT_EQ(instanceName, "Enemy");

    std::vector<fr::Entity> children;
    mRegistry->TryGetComponents<fg::HierarchyComponent>(
        instance, [&](fg::HierarchyComponent &hierarchy) { children = hierarchy.children; });
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(mRegistry->GetParent(children.front()), instance);

    mRegistry->TryGetComponents<fg::TransformComponent>(
        children.front(), [](fg::TransformComponent &transform) {
            EXPECT_NEAR(transform.position.x, 1.0f, 1e-4f);
            EXPECT_NEAR(transform.position.y, 2.0f, 1e-4f);
            EXPECT_NEAR(transform.position.z, 3.0f, 1e-4f);
        });

    EXPECT_TRUE(mRegistry->HasComponent<fg::LightComponent>(children.front()));
    EXPECT_TRUE(mRegistry->HasComponent<fg::MeshComponent>(instance));
}

TEST_F(PrefabSpec, Instantiate_ParentsRootUnderSelection)
{
    const auto root = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Crate"},
        fg::TransformComponent {.position = {4.0f, 0.0f, 0.0f}},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Cube)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    const auto holder = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Holder"}, fg::TransformComponent {.position = {2.0f, 0.0f, 0.0f}});
    mRegistry->ExecuteTasks();

    std::string json;
    ASSERT_TRUE(fg::Prefab::Serialize(*mScene, root, json));

    fr::Entity instance = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Instantiate(*mScene, json, holder, instance));
    EXPECT_EQ(mRegistry->GetParent(instance), holder);
}

TEST_F(PrefabSpec, SaveLoad_AttachesPrefabComponent)
{
    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Prop"},
        fg::TransformComponent {.position = {0.0f, 1.0f, 0.0f}},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Sphere)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    mRegistry->ExecuteTasks();

    const auto path = std::filesystem::temp_directory_path() / "frigga_prefab_spec.prefab";
    std::filesystem::remove(path);
    ASSERT_TRUE(fg::Prefab::Save(*mScene, entity, path));

    fr::Entity instance = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Load(*mScene, path, fr::NullEntity, instance));
    ASSERT_TRUE(mRegistry->HasComponent<fg::PrefabComponent>(instance));
    mRegistry->TryGetComponents<fg::PrefabComponent>(instance, [&](fg::PrefabComponent &prefab) {
        EXPECT_FALSE(prefab.source.empty());
        EXPECT_NE(prefab.source.find("frigga_prefab_spec.prefab"), std::string::npos);
    });

    std::string sceneJson;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, sceneJson));
    EXPECT_NE(sceneJson.find("frigga_prefab_spec.prefab"), std::string::npos);

    std::filesystem::remove(path);
}

TEST(PrefabHelpers, SanitizeFileStem)
{
    EXPECT_EQ(fg::Prefab::SanitizeFileStem("Enemy Head").string(), "Enemy_Head");
    EXPECT_EQ(fg::Prefab::SanitizeFileStem("").string(), "Prefab");
    EXPECT_TRUE(fg::Prefab::IsPrefabExtension(".prefab"));
    EXPECT_TRUE(fg::Prefab::IsPrefabExtension(".PREFAB"));
    EXPECT_FALSE(fg::Prefab::IsPrefabExtension(".json"));
}

TEST(AssetRegistryResources, SetResourcesRoot_RetargetsLookups)
{
    struct ResetOnExit
    {
        ~ResetOnExit()
        {
            fg::AssetRegistry::ResetResourcesRoot();
        }
    } reset;

    const auto dir = std::filesystem::temp_directory_path() / "frigga_project_resources";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "Prefabs");
    std::filesystem::create_directories(dir / "Textures");

    fg::AssetRegistry::SetResourcesRoot(dir);
    EXPECT_EQ(fg::AssetRegistry::ResourcesRoot(), dir);
    EXPECT_EQ(fg::Prefab::DefaultDirectory(), dir / "Prefabs");
    EXPECT_EQ(fg::AssetRegistry::ToAbsoluteResourcePath(std::filesystem::path {"Models"} / "car.glb"),
              dir / "Models" / "car.glb");

    const auto nested = dir / "Textures" / "albedo.png";
    {
        std::ofstream file(nested);
        file << "x";
    }
    EXPECT_EQ(fg::AssetRegistry::MakeRelativeToResources(nested).generic_string(),
              "Textures/albedo.png");

    fg::AssetRegistry::ResetResourcesRoot();
    EXPECT_EQ(fg::AssetRegistry::ResourcesRoot(), fg::AssetRegistry::EngineResourcesRoot());
    std::filesystem::remove_all(dir);
}

TEST(PrefabHelpers, UniqueAssetPathSkipsExisting)
{
    const auto dir = std::filesystem::temp_directory_path() / "frigga_prefab_unique";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    const auto first = fg::Prefab::UniqueAssetPath(dir, "Enemy");
    EXPECT_EQ(first.filename().string(), "Enemy.prefab");
    {
        std::ofstream file(first);
        file << "{}\n";
    }
    const auto second = fg::Prefab::UniqueAssetPath(dir, "Enemy");
    EXPECT_EQ(second.filename().string(), "Enemy_2.prefab");

    std::filesystem::remove_all(dir);
}

TEST_F(PrefabSpec, PrefabCache_GetOrLoad_HitsDiskOnce)
{
    const auto path = std::filesystem::temp_directory_path() / "frigga_prefab_cache_hit.prefab";
    std::filesystem::remove(path);

    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Cached"},
        fg::TransformComponent {},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Cube)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    mRegistry->ExecuteTasks();
    ASSERT_TRUE(fg::Prefab::Save(*mScene, entity, path));

    auto &cache = fg::PrefabCache::Instance();
    cache.InvalidatePath(path);
    const auto loadsBefore = cache.DiskLoadCount();

    fr::Entity first = fr::NullEntity;
    fr::Entity second = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Load(*mScene, path, fr::NullEntity, first));
    ASSERT_TRUE(fg::Prefab::Load(*mScene, path, fr::NullEntity, second));
    EXPECT_EQ(cache.DiskLoadCount(), loadsBefore + 1u);
    EXPECT_TRUE(cache.Contains(fg::PrefabCache::MakeCacheKey(path)));

    std::filesystem::remove(path);
}

TEST_F(PrefabSpec, PrefabCache_Invalidate_ReloadsFromDisk)
{
    const auto path =
        std::filesystem::temp_directory_path() / "frigga_prefab_cache_invalidate.prefab";
    std::filesystem::remove(path);

    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Original"},
        fg::TransformComponent {},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Cube)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    mRegistry->ExecuteTasks();
    ASSERT_TRUE(fg::Prefab::Save(*mScene, entity, path));

    auto &cache = fg::PrefabCache::Instance();
    cache.InvalidatePath(path);

    fr::Entity first = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Load(*mScene, path, fr::NullEntity, first));

    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << R"({"version":7,"entities":[{"name":"Rewritten","transform":{"position":[0,0,0],"scale":[1,1,1],"rotation":[1,0,0,0]},"mesh":{"primitive":"Cube"},"material":{"default":true}}]})";
        file << '\n';
    }

    // Stale cache still serves the previous JSON until Invalidate.
    fr::Entity stale = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Load(*mScene, path, fr::NullEntity, stale));
    std::string staleName;
    mRegistry->TryGetComponents<fg::NameComponent>(
        stale, [&](fg::NameComponent &name) { staleName = name.name; });
    EXPECT_EQ(staleName, "Original");

    cache.InvalidatePath(path);
    fr::Entity fresh = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Load(*mScene, path, fr::NullEntity, fresh));
    std::string freshName;
    mRegistry->TryGetComponents<fg::NameComponent>(
        fresh, [&](fg::NameComponent &name) { freshName = name.name; });
    EXPECT_EQ(freshName, "Rewritten");

    std::filesystem::remove(path);
}

TEST_F(PrefabSpec, PrefabCache_ClearCatalog_DropsEntries)
{
    const auto path = std::filesystem::temp_directory_path() / "frigga_prefab_cache_clear.prefab";
    std::filesystem::remove(path);

    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = "ClearMe"},
        fg::TransformComponent {},
        fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Cube)},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    mRegistry->ExecuteTasks();
    ASSERT_TRUE(fg::Prefab::Save(*mScene, entity, path));

    auto &cache = fg::PrefabCache::Instance();
    cache.InvalidatePath(path);
    fr::Entity instance = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Load(*mScene, path, fr::NullEntity, instance));
    ASSERT_TRUE(cache.Contains(fg::PrefabCache::MakeCacheKey(path)));

    mAssets->ClearCatalog();
    EXPECT_FALSE(cache.Contains(fg::PrefabCache::MakeCacheKey(path)));
    EXPECT_EQ(cache.Size(), 0u);

    std::filesystem::remove(path);
}

TEST_F(PrefabSpec, SharedMaterial_DedupsIdenticalCreateInfo)
{
    fra::MaterialCreateInfo info {};
    info.albedoFactor    = {0.8f, 0.2f, 0.1f, 1.0f};
    info.roughnessFactor = 0.35f;

    const auto first  = mAssets->GetOrCreateSharedMaterial(info);
    const auto second = mAssets->GetOrCreateSharedMaterial(info);
    EXPECT_EQ(first, second);
    EXPECT_TRUE(mAssets->IsSharedMaterial(first));

    info.roughnessFactor = 0.9f;
    const auto third = mAssets->GetOrCreateSharedMaterial(info);
    EXPECT_NE(third, first);
    EXPECT_TRUE(mAssets->IsSharedMaterial(third));
}

TEST_F(PrefabSpec, Instantiate_SharesNonDefaultMaterials)
{
    constexpr std::string_view kPrefabJson = R"({
        "version": 7,
        "entities": [
            {
                "name": "Painted",
                "transform": {
                    "position": [0.0, 0.0, 0.0],
                    "scale": [1.0, 1.0, 1.0],
                    "rotation": [1.0, 0.0, 0.0, 0.0]
                },
                "mesh": { "primitive": "Cube" },
                "material": {
                    "default": false,
                    "albedoFactor": [0.1, 0.7, 0.2, 1.0],
                    "roughnessFactor": 0.42
                }
            }
        ]
    })";

    fr::Entity a = fr::NullEntity;
    fr::Entity b = fr::NullEntity;
    ASSERT_TRUE(fg::Prefab::Instantiate(*mScene, kPrefabJson, fr::NullEntity, a));
    ASSERT_TRUE(fg::Prefab::Instantiate(*mScene, kPrefabJson, fr::NullEntity, b));
    ASSERT_NE(a, b);

    std::uint32_t materialA = 0;
    std::uint32_t materialB = 0;
    mRegistry->TryGetComponents<fg::MaterialComponent>(
        a, [&](fg::MaterialComponent &material) { materialA = material.materialId; });
    mRegistry->TryGetComponents<fg::MaterialComponent>(
        b, [&](fg::MaterialComponent &material) { materialB = material.materialId; });
    EXPECT_NE(materialA, 0u);
    EXPECT_EQ(materialA, materialB);
    EXPECT_TRUE(mAssets->IsSharedMaterial(materialA));
    EXPECT_NE(materialA, mPrimitives->GetDefaultMaterial());
}
