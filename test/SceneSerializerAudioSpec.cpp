#include "EmptyApp.hpp"

#include <Frigga/ECS/Components/AudioSourceComponent.hpp>
#include <Frigga/ECS/Components/HierarchyComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSerializer.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Logging/Logger.hpp>
#include <gtest/gtest.h>

class SceneSerializerAudioSpec: public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                       freyr.WithComponent<fg::NameComponent>()
                           .WithComponent<fg::HierarchyComponent>()
                           .WithComponent<fg::TransformComponent>()
                           .WithComponent<fg::AudioSourceComponent>()
                           .WithComponent<fg::AudioListenerComponent>();
                   })
                   .Build<EmptyApp>();

        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
        mLogger   = skr::MakeArc<skr::Logger<fg::Scene>>(skr::MakeArc<skr::LoggerOptions>());
        mPrimitives = skr::MakeArc<fg::PrimitiveMeshFactory>(fg::PrimitiveMeshFactory::Catalog);
        mAssets     = skr::MakeArc<fg::AssetRegistry>(fg::AssetRegistry::Catalog);
        mUserComponents = skr::MakeArc<fg::UserComponentRegistry>();
        mScene =
            skr::MakeArc<fg::Scene>(skr::Arc<fra::Renderer> {}, mLogger, mRegistry, mPrimitives,
                                    mAssets, mUserComponents);
    }

    skr::Arc<EmptyApp> mApp;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<skr::Logger<fg::Scene>> mLogger;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fg::UserComponentRegistry> mUserComponents;
    skr::Arc<fg::Scene> mScene;
};

TEST_F(SceneSerializerAudioSpec, RoundTripsAudioComponents)
{
    (void)mRegistry->CreateEntity(
        fg::NameComponent {.name = "SFX Source"},
        fg::TransformComponent {.position = {1.0f, 2.0f, 3.0f}},
        fg::AudioSourceComponent {
            .eventPath   = "event:/SFX/Explosion",
            .volume      = 0.8f,
            .pitch       = 1.1f,
            .playOnAwake = true,
            .loop        = false,
            .is3D        = true,
            .minDistance = 2.0f,
            .maxDistance = 40.0f,
        });

    (void)mRegistry->CreateEntity(fg::NameComponent {.name = "Listener"}, fg::TransformComponent {},
                                  fg::AudioListenerComponent {.active = true});

    mRegistry->ExecuteTasks();

    std::string json;
    ASSERT_TRUE(fg::SceneSerializer::Serialize(*mScene, json));
    ASSERT_TRUE(mScene->RestoreSnapshot(json));

    bool foundSource   = false;
    bool foundListener = false;

    mRegistry->CreateMutation()->Each<fg::NameComponent>(
        [&](auto entity, fg::NameComponent &name) {
            if(name.name == "SFX Source")
            {
                mRegistry->TryGetComponents<fg::AudioSourceComponent>(
                    entity, [&](fg::AudioSourceComponent &source) {
                        EXPECT_EQ(source.eventPath, "event:/SFX/Explosion");
                        EXPECT_FLOAT_EQ(source.volume, 0.8f);
                        EXPECT_FLOAT_EQ(source.pitch, 1.1f);
                        EXPECT_TRUE(source.playOnAwake);
                        EXPECT_FALSE(source.loop);
                        EXPECT_TRUE(source.is3D);
                        EXPECT_FLOAT_EQ(source.minDistance, 2.0f);
                        EXPECT_FLOAT_EQ(source.maxDistance, 40.0f);
                        foundSource = true;
                    });
            }
            if(name.name == "Listener")
            {
                mRegistry->TryGetComponents<fg::AudioListenerComponent>(
                    entity, [&](fg::AudioListenerComponent &listener) {
                        EXPECT_TRUE(listener.active);
                        foundListener = true;
                    });
            }
        });

    EXPECT_TRUE(foundSource);
    EXPECT_TRUE(foundListener);
}
