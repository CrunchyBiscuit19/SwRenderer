#pragma once

#include <Data/SwMaterial.h>

#include <glm/glm.hpp>

struct SwVertex {
    glm::vec3 mPosition;
    glm::vec3 mNormal;
    glm::vec4 mColor;
    glm::vec2 mUv;

    SwVertex(glm::vec3 position, glm::vec3 normal, glm::vec4 color, glm::vec2 uv);
};

struct SwBounds {
    glm::vec3 mMin;
    glm::vec3 mMax;

    SwBounds() = default;
    SwBounds(glm::vec3 min, glm::vec3 max);
};

struct SwPrimitive {
    std::uint32_t mRelativeFirstIndex;
    std::uint32_t mIndexCount;
    std::uint32_t mRelativeVertexOffset;
    SwMaterial& mMaterial;

    SwPrimitive(std::uint32_t relativeFirstIndex, std::uint32_t indexCount, std::uint32_t relativeVertexOffset, SwMaterial& material);
};

class SwMesh {
private:
    static const std::uint32_t sLatestMeshId{0};
    std::uint32_t mId;
    std::string mName;
    std::vector<SwPrimitive> mPrimitives;
    SwBounds mBounds;
    std::uint32_t mRelativeFirstBounds;
    SwAllocatedBuffer mVertexBuffer;
    std::uint32_t mNumVertices{0};
    std::uint32_t mVertexOffsetInScene{0};
    SwAllocatedBuffer mIndexBuffer;
    std::uint32_t mNumIndices{0};
    std::uint32_t mFirstIndexInScene{0};

public:
    SwMesh(fastgltf::Mesh& mesh);
};