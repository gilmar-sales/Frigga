#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/EditorCamera.hpp"

#include <Freya/Core/Renderer.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

namespace FRIGGA_NAMESPACE
{

    class Scene
    {
      public:
        Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
              const skr::Arc<fr::Registry> &ecsRegistry,
              const skr::Arc<PrimitiveMeshFactory> &primitives);
        ~Scene() = default;

        void Update(float ts);
        void OnEditorRender(float ts);

        [[nodiscard]] fr::Entity GetMainCameraEntity() const
        {
            return mMainCameraEntity;
        }

        [[nodiscard]] bool IsMainCamera(fr::Entity entity) const
        {
            return entity == mMainCameraEntity;
        }

        [[nodiscard]] EditorCamera &GetEditorCamera()
        {
            return mEditorCamera;
        }

        [[nodiscard]] const EditorCamera &GetEditorCamera() const
        {
            return mEditorCamera;
        }

        void PreferEditorCamera()
        {
            mUseEditorCamera = true;
        }

        void PreferGameplayCamera()
        {
            mUseEditorCamera = false;
        }

        [[nodiscard]] bool IsUsingEditorCamera() const
        {
            return mUseEditorCamera;
        }

      private:
        skr::Arc<fr::Registry> mEcsRegistry;
        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<skr::Logger<Scene>> mLogger;
        skr::Arc<PrimitiveMeshFactory> mPrimitives;
        fr::Entity mMainCameraEntity {};
        EditorCamera mEditorCamera {};
        bool mUseEditorCamera = true;
    };

} // namespace FRIGGA_NAMESPACE
