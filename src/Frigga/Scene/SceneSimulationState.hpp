#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Physics/IPhysicsWorld.hpp"
#include "Frigga/Scene/Scene.hpp"

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    enum class SimulationMode : std::uint8_t
    {
        Edit = 0,
        Play,
    };

    class SceneSimulationState
    {
      public:
        SceneSimulationState(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<IPhysicsWorld> &physicsWorld,
                             const skr::Arc<Scene> &scene,
                             const skr::Arc<PrimitiveMeshFactory> &primitives,
                             const skr::Arc<skr::Logger<SceneSimulationState>> &logger);

        [[nodiscard]] bool IsPlaying() const
        {
            return mMode == SimulationMode::Play;
        }

        [[nodiscard]] SimulationMode GetMode() const
        {
            return mMode;
        }

        /// Cleared after a successful read. Used by the Gameplay viewport to steal focus on Play.
        [[nodiscard]] bool ConsumeFocusGameplayRequest()
        {
            const bool requested = mFocusGameplayRequested;
            mFocusGameplayRequested = false;
            return requested;
        }

        /// Cleared after a successful read. Used by the Editor viewport to steal focus on Stop.
        [[nodiscard]] bool ConsumeFocusEditorRequest()
        {
            const bool requested = mFocusEditorRequested;
            mFocusEditorRequested = false;
            return requested;
        }

        void Play();
        void Stop();
        void Toggle();

      private:
        void snapshotTransforms();
        void restoreTransforms();
        void buildPhysicsWorld();
        void teardownPhysicsWorld();
        PhysicsBodyDesc makeBodyDesc(const TransformComponent &transform,
                                     const RigidBodyComponent &rigidBody,
                                     fr::Entity entity) const;

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<IPhysicsWorld> mPhysicsWorld;
        skr::Arc<Scene> mScene;
        skr::Arc<PrimitiveMeshFactory> mPrimitives;
        skr::Arc<skr::Logger<SceneSimulationState>> mLogger;
        SimulationMode mMode = SimulationMode::Edit;
        bool mFocusGameplayRequested = false;
        bool mFocusEditorRequested   = false;
        std::unordered_map<fr::Entity, TransformComponent> mEditTransforms;
    };

} // namespace FRIGGA_NAMESPACE
