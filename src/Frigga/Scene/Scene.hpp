#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/EditorCamera.hpp"

#include <Freya/Core/Renderer.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    class SceneSerializer;

    class Scene
    {
      public:
        Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
              const skr::Arc<fr::Registry> &ecsRegistry,
              const skr::Arc<PrimitiveMeshFactory> &primitives);
        ~Scene() = default;

        void Update(float ts);
        void OnEditorRender(float ts);

        void NewScene();
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
        friend class SceneSerializer;

        void ClearEntities();
        void CreateDefaultEntities();
        void FlushEcs();

        skr::Arc<fr::Registry> mEcsRegistry;
        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<skr::Logger<Scene>> mLogger;
        skr::Arc<PrimitiveMeshFactory> mPrimitives;
        fr::Entity mMainCameraEntity {};
        EditorCamera mEditorCamera {};
        bool mUseEditorCamera = true;
        std::filesystem::path mPath {};
    };

} // namespace FRIGGA_NAMESPACE
