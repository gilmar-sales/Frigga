#include "PrimitiveMeshFactory.hpp"

#include <cmath>
#include <numbers>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        fra::Vertex makeVertex(const glm::vec3 &position, const glm::vec3 &normal,
                               const glm::vec2 &uv,
                               const glm::vec3 &color = glm::vec3(0.55f))
        {
            glm::vec3 tangent = glm::cross(normal, glm::vec3(0.0f, 1.0f, 0.0f));
            if(glm::dot(tangent, tangent) < 1e-6f)
            {
                tangent = glm::cross(normal, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            tangent = glm::normalize(tangent);

            return fra::Vertex {
                .position = position,
                .color    = color,
                .normal   = glm::normalize(normal),
                .tangent  = tangent,
                .texCoord = uv,
            };
        }

        void pushQuad(std::vector<fra::Vertex> &vertices, std::vector<std::uint16_t> &indices,
                      const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2,
                      const glm::vec3 &p3, const glm::vec3 &normal)
        {
            const auto base = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back(makeVertex(p0, normal, {0.0f, 1.0f}));
            vertices.push_back(makeVertex(p1, normal, {1.0f, 1.0f}));
            vertices.push_back(makeVertex(p2, normal, {1.0f, 0.0f}));
            vertices.push_back(makeVertex(p3, normal, {0.0f, 0.0f}));
            indices.insert(indices.end(),
                           {base, static_cast<std::uint16_t>(base + 1),
                            static_cast<std::uint16_t>(base + 2), base,
                            static_cast<std::uint16_t>(base + 2),
                            static_cast<std::uint16_t>(base + 3)});
        }
    } // namespace

    PrimitiveMeshFactory::PrimitiveMeshFactory(const skr::Arc<fra::MeshPool> &meshPool,
                                               const skr::Arc<fra::MaterialPool> &materialPool,
                                               const skr::Arc<fra::TexturePool> &texturePool)
        : mMeshPool(meshPool), mMaterialPool(materialPool), mTexturePool(texturePool)
    {
        createDefaultMaterial();
    }

    PrimitiveMeshFactory::PrimitiveMeshFactory(CatalogTag)
    {
        for(std::size_t i = 0; i < mMeshes.size(); ++i)
        {
            mMeshes[i]  = static_cast<std::uint32_t>(i + 1);
            mCreated[i] = true;
        }
        mDefaultMaterial    = 1;
        mCatalogMaterialSeq = 1;
    }

    void PrimitiveMeshFactory::createDefaultMaterial()
    {
        const auto albedo =
            mTexturePool->CreateTextureFromFile("./Resources/Textures/default_gray.png");
        const auto roughness =
            mTexturePool->CreateTextureFromFile("./Resources/Textures/default_roughness.png");

        mDefaultMaterial = mMaterialPool->Create({
            .albedo    = albedo,
            .roughness = roughness,
        });
    }

    std::uint32_t PrimitiveMeshFactory::GetDefaultMaterial() const
    {
        return mDefaultMaterial;
    }

    std::uint32_t PrimitiveMeshFactory::CreateMaterial(const fra::MaterialCreateInfo &createInfo)
    {
        if(mMaterialPool == nullptr)
        {
            return ++mCatalogMaterialSeq;
        }
        return mMaterialPool->Create(createInfo);
    }

    std::uint32_t PrimitiveMeshFactory::DuplicateMaterial(std::uint32_t materialId)
    {
        return CreateMaterial(GetMaterialCreateInfo(materialId));
    }

    void PrimitiveMeshFactory::UpdateMaterial(std::uint32_t materialId,
                                              const fra::MaterialCreateInfo &createInfo)
    {
        if(mMaterialPool == nullptr)
        {
            return;
        }
        mMaterialPool->Update(materialId, createInfo);
    }

    fra::MaterialCreateInfo PrimitiveMeshFactory::GetMaterialCreateInfo(
        std::uint32_t materialId) const
    {
        if(mMaterialPool == nullptr)
        {
            return {};
        }
        return mMaterialPool->GetCreateInfo(materialId);
    }

    std::uint32_t PrimitiveMeshFactory::GetMesh(PrimitiveType type)
    {
        const auto index = static_cast<std::size_t>(type);
        if(mCreated[index])
        {
            return mMeshes[index];
        }

        switch(type)
        {
            case PrimitiveType::Cube:
                mMeshes[index] = createCube();
                break;
            case PrimitiveType::Sphere:
                mMeshes[index] = createSphere(32, 16);
                break;
            case PrimitiveType::Capsule:
                mMeshes[index] = createCapsule(24, 8);
                break;
            case PrimitiveType::Cylinder:
                mMeshes[index] = createCylinder(32);
                break;
            case PrimitiveType::Cone:
                mMeshes[index] = createCone(32);
                break;
            case PrimitiveType::Plane:
                mMeshes[index] = createPlane();
                break;
            case PrimitiveType::Quad:
                mMeshes[index] = createQuad();
                break;
            case PrimitiveType::Count:
                break;
        }

        mCreated[index] = true;
        return mMeshes[index];
    }

    bool PrimitiveMeshFactory::TryFindPrimitive(std::uint32_t meshId, PrimitiveType &outType) const
    {
        for(std::size_t i = 0; i < mMeshes.size(); ++i)
        {
            if(mCreated[i] && mMeshes[i] == meshId)
            {
                outType = static_cast<PrimitiveType>(i);
                return true;
            }
        }
        return false;
    }

    const char *PrimitiveMeshFactory::GetDisplayName(PrimitiveType type)
    {
        switch(type)
        {
            case PrimitiveType::Cube:
                return "Cube";
            case PrimitiveType::Sphere:
                return "Sphere";
            case PrimitiveType::Capsule:
                return "Capsule";
            case PrimitiveType::Cylinder:
                return "Cylinder";
            case PrimitiveType::Cone:
                return "Cone";
            case PrimitiveType::Plane:
                return "Plane";
            case PrimitiveType::Quad:
                return "Quad";
            case PrimitiveType::Count:
                return "Unknown";
        }
        return "Unknown";
    }

    bool PrimitiveMeshFactory::TryParsePrimitive(std::string_view name, PrimitiveType &outType)
    {
        for(std::uint8_t i = 0; i < static_cast<std::uint8_t>(PrimitiveType::Count); ++i)
        {
            const auto type = static_cast<PrimitiveType>(i);
            if(name == GetDisplayName(type))
            {
                outType = type;
                return true;
            }
        }
        return false;
    }

    std::vector<glm::vec3> PrimitiveMeshFactory::GetColliderHullPoints(PrimitiveType type)
    {
        switch(type)
        {
        case PrimitiveType::Cube:
            return {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
                    {-0.5f, 0.5f, -0.5f},  {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},
                    {0.5f, 0.5f, 0.5f},   {-0.5f, 0.5f, 0.5f}};
        case PrimitiveType::Sphere:
        {
            std::vector<glm::vec3> points;
            constexpr int stacks = 8;
            constexpr int slices = 12;
            for(int y = 0; y <= stacks; ++y)
            {
                const float v = static_cast<float>(y) / static_cast<float>(stacks);
                const float phi = v * std::numbers::pi_v<float>;
                for(int x = 0; x < slices; ++x)
                {
                    const float u = static_cast<float>(x) / static_cast<float>(slices);
                    const float theta = u * 2.0f * std::numbers::pi_v<float>;
                    points.emplace_back(0.5f * std::sin(phi) * std::cos(theta),
                                        0.5f * std::cos(phi),
                                        0.5f * std::sin(phi) * std::sin(theta));
                }
            }
            return points;
        }
        case PrimitiveType::Capsule:
        {
            std::vector<glm::vec3> points;
            constexpr int slices = 12;
            for(int i = 0; i < slices; ++i)
            {
                const float a = (static_cast<float>(i) / slices) * 2.0f * std::numbers::pi_v<float>;
                const float x = 0.5f * std::cos(a);
                const float z = 0.5f * std::sin(a);
                points.emplace_back(x, -0.5f, z);
                points.emplace_back(x, 0.5f, z);
                points.emplace_back(x, -1.0f, z);
                points.emplace_back(x, 1.0f, z);
            }
            points.emplace_back(0.0f, -1.0f, 0.0f);
            points.emplace_back(0.0f, 1.0f, 0.0f);
            return points;
        }
        case PrimitiveType::Cylinder:
        {
            std::vector<glm::vec3> points;
            constexpr int slices = 16;
            for(int i = 0; i < slices; ++i)
            {
                const float a = (static_cast<float>(i) / slices) * 2.0f * std::numbers::pi_v<float>;
                const float x = 0.5f * std::cos(a);
                const float z = 0.5f * std::sin(a);
                points.emplace_back(x, -0.5f, z);
                points.emplace_back(x, 0.5f, z);
            }
            return points;
        }
        case PrimitiveType::Cone:
        {
            std::vector<glm::vec3> points;
            points.emplace_back(0.0f, 0.5f, 0.0f);
            constexpr int slices = 16;
            for(int i = 0; i < slices; ++i)
            {
                const float a = (static_cast<float>(i) / slices) * 2.0f * std::numbers::pi_v<float>;
                points.emplace_back(0.5f * std::cos(a), -0.5f, 0.5f * std::sin(a));
            }
            return points;
        }
        case PrimitiveType::Plane:
            // Must match createPlane() extents (-5..5 on XZ). Thin Y for a valid convex hull.
            return {{-5.0f, 0.0f, -5.0f}, {5.0f, 0.0f, -5.0f}, {5.0f, 0.0f, 5.0f},
                    {-5.0f, 0.0f, 5.0f},  {-5.0f, 0.02f, -5.0f}, {5.0f, 0.02f, -5.0f},
                    {5.0f, 0.02f, 5.0f}, {-5.0f, 0.02f, 5.0f}};
        case PrimitiveType::Quad:
            // Must match createQuad() extents (-0.5..0.5 on XY), extruded slightly on Z.
            return {{-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f},
                    {-0.5f, 0.5f, 0.0f},  {-0.5f, -0.5f, 0.02f}, {0.5f, -0.5f, 0.02f},
                    {0.5f, 0.5f, 0.02f}, {-0.5f, 0.5f, 0.02f}};
        case PrimitiveType::Count:
            break;
        }
        return {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
                {-0.5f, 0.5f, -0.5f},  {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},
                {0.5f, 0.5f, 0.5f},   {-0.5f, 0.5f, 0.5f}};
    }

    std::uint32_t PrimitiveMeshFactory::createCube()
    {
        std::vector<fra::Vertex> vertices;
        std::vector<std::uint16_t> indices;
        vertices.reserve(24);
        indices.reserve(36);

        pushQuad(vertices, indices, {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
                 {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f});
        pushQuad(vertices, indices, {0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f},
                 {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f});
        pushQuad(vertices, indices, {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f},
                 {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f});
        pushQuad(vertices, indices, {0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f},
                 {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});
        pushQuad(vertices, indices, {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f},
                 {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f});
        pushQuad(vertices, indices, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
                 {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f});

        return mMeshPool->CreateMesh(vertices, indices);
    }

    std::uint32_t PrimitiveMeshFactory::createSphere(std::uint32_t segments, std::uint32_t rings)
    {
        std::vector<fra::Vertex> vertices;
        std::vector<std::uint16_t> indices;

        constexpr float pi = std::numbers::pi_v<float>;
        for(std::uint32_t ring = 0; ring <= rings; ++ring)
        {
            const float v     = static_cast<float>(ring) / static_cast<float>(rings);
            const float phi   = v * pi;
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);

            for(std::uint32_t segment = 0; segment <= segments; ++segment)
            {
                const float u     = static_cast<float>(segment) / static_cast<float>(segments);
                const float theta = u * 2.0f * pi;
                const glm::vec3 normal {std::cos(theta) * sinPhi, cosPhi,
                                        std::sin(theta) * sinPhi};
                vertices.push_back(makeVertex(normal * 0.5f, normal, {u, v}));
            }
        }

        for(std::uint32_t ring = 0; ring < rings; ++ring)
        {
            for(std::uint32_t segment = 0; segment < segments; ++segment)
            {
                const auto current =
                    static_cast<std::uint16_t>(ring * (segments + 1) + segment);
                const auto next = static_cast<std::uint16_t>(current + segments + 1);
                indices.insert(indices.end(),
                               {current, next, static_cast<std::uint16_t>(current + 1), next,
                                static_cast<std::uint16_t>(next + 1),
                                static_cast<std::uint16_t>(current + 1)});
            }
        }

        return mMeshPool->CreateMesh(vertices, indices);
    }

    std::uint32_t PrimitiveMeshFactory::createCapsule(std::uint32_t segments, std::uint32_t rings)
    {
        std::vector<fra::Vertex> vertices;
        std::vector<std::uint16_t> indices;

        constexpr float pi       = std::numbers::pi_v<float>;
        constexpr float radius   = 0.5f;
        constexpr float cylinder = 1.0f;
        const float halfHeight   = cylinder * 0.5f;
        std::uint32_t ringCount  = 0;

        auto addLatLongRing = [&](float y, float phi, float v) {
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);
            for(std::uint32_t segment = 0; segment <= segments; ++segment)
            {
                const float u     = static_cast<float>(segment) / static_cast<float>(segments);
                const float theta = u * 2.0f * pi;
                const glm::vec3 normal {std::cos(theta) * sinPhi, cosPhi,
                                        std::sin(theta) * sinPhi};
                const glm::vec3 position {normal.x * radius, y + normal.y * radius,
                                          normal.z * radius};
                vertices.push_back(makeVertex(position, normal, {u, v}));
            }
            ++ringCount;
        };

        auto addCylinderRing = [&](float y, float v) {
            for(std::uint32_t segment = 0; segment <= segments; ++segment)
            {
                const float u     = static_cast<float>(segment) / static_cast<float>(segments);
                const float theta = u * 2.0f * pi;
                const glm::vec3 normal {std::cos(theta), 0.0f, std::sin(theta)};
                const glm::vec3 position {normal.x * radius, y, normal.z * radius};
                vertices.push_back(makeVertex(position, normal, {u, v}));
            }
            ++ringCount;
        };

        for(std::uint32_t ring = 0; ring <= rings; ++ring)
        {
            const float t   = static_cast<float>(ring) / static_cast<float>(rings);
            const float phi = t * (pi * 0.5f);
            addLatLongRing(halfHeight, phi, t * 0.25f);
        }

        addCylinderRing(-halfHeight, 0.75f);

        for(std::uint32_t ring = 1; ring <= rings; ++ring)
        {
            const float t   = static_cast<float>(ring) / static_cast<float>(rings);
            const float phi = (pi * 0.5f) + t * (pi * 0.5f);
            addLatLongRing(-halfHeight, phi, 0.75f + t * 0.25f);
        }

        for(std::uint32_t ring = 0; ring + 1 < ringCount; ++ring)
        {
            for(std::uint32_t segment = 0; segment < segments; ++segment)
            {
                const auto current =
                    static_cast<std::uint16_t>(ring * (segments + 1) + segment);
                const auto next = static_cast<std::uint16_t>(current + segments + 1);
                indices.insert(indices.end(),
                               {current, next, static_cast<std::uint16_t>(current + 1), next,
                                static_cast<std::uint16_t>(next + 1),
                                static_cast<std::uint16_t>(current + 1)});
            }
        }

        return mMeshPool->CreateMesh(vertices, indices);
    }

    std::uint32_t PrimitiveMeshFactory::createCylinder(std::uint32_t segments)
    {
        std::vector<fra::Vertex> vertices;
        std::vector<std::uint16_t> indices;

        constexpr float pi     = std::numbers::pi_v<float>;
        constexpr float radius = 0.5f;
        constexpr float halfH  = 0.5f;

        for(std::uint32_t segment = 0; segment <= segments; ++segment)
        {
            const float u     = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = u * 2.0f * pi;
            const float x     = std::cos(theta) * radius;
            const float z     = std::sin(theta) * radius;
            const glm::vec3 normal {std::cos(theta), 0.0f, std::sin(theta)};

            vertices.push_back(makeVertex({x, -halfH, z}, normal, {u, 1.0f}));
            vertices.push_back(makeVertex({x, halfH, z}, normal, {u, 0.0f}));
        }

        for(std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const auto i = static_cast<std::uint16_t>(segment * 2);
            indices.insert(indices.end(),
                           {i, static_cast<std::uint16_t>(i + 1),
                            static_cast<std::uint16_t>(i + 2),
                            static_cast<std::uint16_t>(i + 1),
                            static_cast<std::uint16_t>(i + 3),
                            static_cast<std::uint16_t>(i + 2)});
        }

        const auto topCenter =
            static_cast<std::uint16_t>(vertices.size());
        vertices.push_back(makeVertex({0.0f, halfH, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}));
        const auto bottomCenter =
            static_cast<std::uint16_t>(vertices.size());
        vertices.push_back(makeVertex({0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}));

        for(std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const float theta0 = (static_cast<float>(segment) / segments) * 2.0f * pi;
            const float theta1 = (static_cast<float>(segment + 1) / segments) * 2.0f * pi;
            const glm::vec3 p0 {std::cos(theta0) * radius, halfH, std::sin(theta0) * radius};
            const glm::vec3 p1 {std::cos(theta1) * radius, halfH, std::sin(theta1) * radius};
            const glm::vec3 p2 {std::cos(theta0) * radius, -halfH, std::sin(theta0) * radius};
            const glm::vec3 p3 {std::cos(theta1) * radius, -halfH, std::sin(theta1) * radius};

            const auto t0 = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back(makeVertex(p0, {0.0f, 1.0f, 0.0f},
                                          {std::cos(theta0) * 0.5f + 0.5f,
                                           std::sin(theta0) * 0.5f + 0.5f}));
            vertices.push_back(makeVertex(p1, {0.0f, 1.0f, 0.0f},
                                          {std::cos(theta1) * 0.5f + 0.5f,
                                           std::sin(theta1) * 0.5f + 0.5f}));
            indices.insert(indices.end(),
                           {topCenter, t0, static_cast<std::uint16_t>(t0 + 1)});

            const auto b0 = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back(makeVertex(p2, {0.0f, -1.0f, 0.0f},
                                          {std::cos(theta0) * 0.5f + 0.5f,
                                           std::sin(theta0) * 0.5f + 0.5f}));
            vertices.push_back(makeVertex(p3, {0.0f, -1.0f, 0.0f},
                                          {std::cos(theta1) * 0.5f + 0.5f,
                                           std::sin(theta1) * 0.5f + 0.5f}));
            indices.insert(indices.end(),
                           {bottomCenter, static_cast<std::uint16_t>(b0 + 1), b0});
        }

        return mMeshPool->CreateMesh(vertices, indices);
    }

    std::uint32_t PrimitiveMeshFactory::createCone(std::uint32_t segments)
    {
        std::vector<fra::Vertex> vertices;
        std::vector<std::uint16_t> indices;

        constexpr float pi     = std::numbers::pi_v<float>;
        constexpr float radius = 0.5f;
        constexpr float halfH  = 0.5f;

        const glm::vec3 apex {0.0f, halfH, 0.0f};
        for(std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const float theta0 = (static_cast<float>(segment) / segments) * 2.0f * pi;
            const float theta1 = (static_cast<float>(segment + 1) / segments) * 2.0f * pi;
            const glm::vec3 p0 {std::cos(theta0) * radius, -halfH, std::sin(theta0) * radius};
            const glm::vec3 p1 {std::cos(theta1) * radius, -halfH, std::sin(theta1) * radius};

            const glm::vec3 normal = glm::normalize(glm::cross(p1 - apex, p0 - apex));
            const auto base        = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back(makeVertex(apex, normal, {0.5f, 0.0f}));
            vertices.push_back(makeVertex(p0, normal, {0.0f, 1.0f}));
            vertices.push_back(makeVertex(p1, normal, {1.0f, 1.0f}));
            indices.insert(indices.end(),
                           {base, static_cast<std::uint16_t>(base + 1),
                            static_cast<std::uint16_t>(base + 2)});
        }

        const auto bottomCenter =
            static_cast<std::uint16_t>(vertices.size());
        vertices.push_back(makeVertex({0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}));
        for(std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const float theta0 = (static_cast<float>(segment) / segments) * 2.0f * pi;
            const float theta1 = (static_cast<float>(segment + 1) / segments) * 2.0f * pi;
            const glm::vec3 p0 {std::cos(theta0) * radius, -halfH, std::sin(theta0) * radius};
            const glm::vec3 p1 {std::cos(theta1) * radius, -halfH, std::sin(theta1) * radius};
            const auto b0 = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back(makeVertex(p0, {0.0f, -1.0f, 0.0f},
                                          {std::cos(theta0) * 0.5f + 0.5f,
                                           std::sin(theta0) * 0.5f + 0.5f}));
            vertices.push_back(makeVertex(p1, {0.0f, -1.0f, 0.0f},
                                          {std::cos(theta1) * 0.5f + 0.5f,
                                           std::sin(theta1) * 0.5f + 0.5f}));
            indices.insert(indices.end(),
                           {bottomCenter, static_cast<std::uint16_t>(b0 + 1), b0});
        }

        return mMeshPool->CreateMesh(vertices, indices);
    }

    std::uint32_t PrimitiveMeshFactory::createPlane()
    {
        std::vector<fra::Vertex> vertices;
        std::vector<std::uint16_t> indices;
        pushQuad(vertices, indices, {-5.0f, 0.0f, 5.0f}, {5.0f, 0.0f, 5.0f},
                 {5.0f, 0.0f, -5.0f}, {-5.0f, 0.0f, -5.0f}, {0.0f, 1.0f, 0.0f});
        // Larger UVs for a tiled plane look
        vertices[0].texCoord = {0.0f, 10.0f};
        vertices[1].texCoord = {10.0f, 10.0f};
        vertices[2].texCoord = {10.0f, 0.0f};
        vertices[3].texCoord = {0.0f, 0.0f};
        return mMeshPool->CreateMesh(vertices, indices);
    }

    std::uint32_t PrimitiveMeshFactory::createQuad()
    {
        std::vector<fra::Vertex> vertices;
        std::vector<std::uint16_t> indices;
        pushQuad(vertices, indices, {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f},
                 {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f});
        return mMeshPool->CreateMesh(vertices, indices);
    }

} // namespace FRIGGA_NAMESPACE
