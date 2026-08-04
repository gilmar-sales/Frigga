#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Physics/IPhysicsWorld.hpp"
#include "Frigga/Scene/Scene.hpp"

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <string>

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

        /// True while the play session is active (running or paused).
        [[nodiscard]] bool IsPlaying() const
        {
            return mMode == SimulationMode::Play;
        }

        [[nodiscard]] bool IsPaused() const
        {
            return IsPlaying() && mPaused;
        }

        /// True when the physics world should advance with wall-clock delta time.
        [[nodiscard]] bool IsRunning() const
        {
            return IsPlaying() && !mPaused;
        }

        [[nodiscard]] SimulationMode GetMode() const
        {
            return mMode;
        }

        [[nodiscard]] bool GetShowColliders() const
        {
            return mShowColliders;
        }

        void SetShowColliders(bool show)
        {
            mShowColliders = show;
        }

        void ToggleShowColliders()
        {
            mShowColliders = !mShowColliders;
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

        /// One fixed physics step on the next PhysicsSystem update (leaves session paused).
        [[nodiscard]] bool ConsumeStepRequest()
        {
            const bool requested = mStepRequested;
            mStepRequested       = false;
            return requested;
        }

        void Play();
        void Pause();
        void Resume();
        void Stop();
        /// Enter play from Edit; pause/resume while already in a play session.
        void TogglePlayPause();
        void Step();

      private:
        void snapshotScene();
        void restoreScene();
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
        bool mPaused                 = false;
        bool mStepRequested          = false;
        bool mShowColliders          = false;
        bool mFocusGameplayRequested = false;
        bool mFocusEditorRequested   = false;
        std::string mEditSceneSnapshot;
    };

} // namespace FRIGGA_NAMESPACE
