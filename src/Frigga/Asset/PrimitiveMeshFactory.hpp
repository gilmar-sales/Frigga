#pragma once

#include <Freya/Freya.hpp>

#include <array>
#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    enum class PrimitiveType : std::uint8_t
    {
        Cube = 0,
        Sphere,
        Capsule,
        Cylinder,
        Cone,
        Plane,
        Quad,
        Count
    };

    class PrimitiveMeshFactory
    {
      public:
        PrimitiveMeshFactory(const skr::Arc<fra::MeshPool> &meshPool,
                             const skr::Arc<fra::MaterialPool> &materialPool,
                             const skr::Arc<fra::TexturePool> &texturePool);

        [[nodiscard]] std::uint32_t GetMesh(PrimitiveType type);
        [[nodiscard]] std::uint32_t GetDefaultMaterial() const;
        [[nodiscard]] static const char *GetDisplayName(PrimitiveType type);

      private:
        void createDefaultMaterial();

        std::uint32_t createCube();
        std::uint32_t createSphere(std::uint32_t segments, std::uint32_t rings);
        std::uint32_t createCapsule(std::uint32_t segments, std::uint32_t rings);
        std::uint32_t createCylinder(std::uint32_t segments);
        std::uint32_t createCone(std::uint32_t segments);
        std::uint32_t createPlane();
        std::uint32_t createQuad();

        skr::Arc<fra::MeshPool> mMeshPool;
        skr::Arc<fra::MaterialPool> mMaterialPool;
        skr::Arc<fra::TexturePool> mTexturePool;
        std::uint32_t mDefaultMaterial = 0;
        std::array<std::uint32_t, static_cast<std::size_t>(PrimitiveType::Count)> mMeshes {};
        std::array<bool, static_cast<std::size_t>(PrimitiveType::Count)> mCreated {};
    };

} // namespace FRIGGA_NAMESPACE
