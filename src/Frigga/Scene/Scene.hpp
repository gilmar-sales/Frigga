#pragma once

#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Scene/EditorCamera.hpp"

#include <Freya/Core/Renderer.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    class SceneSerializer;

    enum class SceneTemplate : std::uint8_t
    {
        D3 = 0,
        D2 = 1,
    };

    class Scene
    {
      public:
        Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
              const skr::Arc<fr::Registry> &ecsRegistry,
              const skr::Arc<PrimitiveMeshFactory> &primitives,
              const skr::Arc<AssetRegistry> &assets,
              const skr::Arc<UserComponentRegistry> &userComponents);

        struct HeadlessTag
        {
        };

        static constexpr HeadlessTag Headless {};

        Scene(HeadlessTag, const skr::Arc<skr::Logger<Scene>> &logger,
              const skr::Arc<fr::Registry> &ecsRegistry,
              const skr::Arc<PrimitiveMeshFactory> &primitives,
              const skr::Arc<AssetRegistry> &assets,
              const skr::Arc<UserComponentRegistry> &userComponents);
        ~Scene() = default;

        void Update(float ts);
        void OnEditorRender(float ts);

        void NewScene();
        void NewSceneFromTemplate(SceneTemplate sceneTemplate);
        bool SaveScene(const std::filesystem::path &path);
        bool SaveScene();
        bool LoadScene(const std::filesystem::path &path);

        /// Serialize the current edit scene without changing path / disk files.
        bool CaptureSnapshot(std::string &outJson);
        /// Replace entities from a previous CaptureSnapshot; preserves the scene path.
        bool RestoreSnapshot(std::string_view json);

        [[nodiscard]] bool HasPath() const
        {
            return !mPath.empty();
        }

        [[nodiscard]] const std::filesystem::path &GetPath() const
        {
            return mPath;
        }

        [[nodiscard]] std::string GetDisplayName() const
        {
            if(mPath.empty())
            {
                return "untitled";
            }
            return mPath.filename().string();
        }

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

        [[nodiscard]] EditorCamera &GetPreviewCamera()
        {
            return mPreviewCamera;
        }

        [[nodiscard]] const EditorCamera &GetPreviewCamera() const
        {
            return mPreviewCamera;
        }

        void PreferEditorCamera()
        {
            mCameraMode = CameraMode::Editor;
        }

        void PreferGameplayCamera()
        {
            mCameraMode = CameraMode::Gameplay;
        }

        void PreferPreviewCamera()
        {
            mCameraMode = CameraMode::Preview;
        }

        [[nodiscard]] bool IsUsingEditorCamera() const
        {
            return mCameraMode == CameraMode::Editor;
        }

        [[nodiscard]] bool IsUsingPreviewCamera() const
        {
            return mCameraMode == CameraMode::Preview;
        }

        void SetRenderIsolation(fr::Entity entity)
        {
            mRenderIsolation = entity;
        }

        void ClearRenderIsolation()
        {
            mRenderIsolation.reset();
        }

        [[nodiscard]] bool HasRenderIsolation() const
        {
            return mRenderIsolation.has_value();
        }

        [[nodiscard]] fr::Entity GetRenderIsolation() const
        {
            return mRenderIsolation.value_or(static_cast<fr::Entity>(-1));
        }

      private:
        enum class CameraMode : std::uint8_t
        {
            Editor = 0,
            Gameplay,
            Preview,
        };

        friend class SceneSerializer;

        void ClearEntities();
        void CreateDefaultEntities();
        void CreateDefaultEntities3D();
        void CreateDefaultEntities2D();
        void FlushEcs();

        skr::Arc<fr::Registry> mEcsRegistry;
        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<skr::Logger<Scene>> mLogger;
        skr::Arc<PrimitiveMeshFactory> mPrimitives;
        skr::Arc<AssetRegistry> mAssets;
        skr::Arc<UserComponentRegistry> mUserComponents;
        fr::Entity mMainCameraEntity {};
        EditorCamera mEditorCamera {};
        EditorCamera mPreviewCamera {};
        CameraMode mCameraMode = CameraMode::Editor;
        std::optional<fr::Entity> mRenderIsolation {};
        std::filesystem::path mPath {};
    };

} // namespace FRIGGA_NAMESPACE
