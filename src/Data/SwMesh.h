#pragma once

#include <Data/SwMaterial.h>

#include <glm/glm.hpp>

class SwAsset;

struct SwVertex {
    glm::vec3 mPosition{0.f, 0.f, 0.f};
    glm::vec3 mNormal{0.f, 1.f, 0.f};
    glm::vec4 mColor{1.f, 1.f, 1.f, 1.f};
    glm::vec2 mUv{0.f, 0.f};
    glm::vec4 mTangent{1.f, 0.f, 0.f, 1.f};  // w = handedness

    SwVertex() = default;
    SwVertex(glm::vec3 position, glm::vec3 normal, glm::vec4 color, glm::vec2 uv);
};

struct SwBounds {
private:
    static constexpr std::uint32_t BOUNDS_STAGING_BUFFER_SIZE{1 << 20};  // 1 MB

public:
    static SwStagingBuffer sBoundsStaging;

    glm::vec3 mMin;
    glm::vec3 mMax;

    SwBounds() = default;
    SwBounds(glm::vec3 min, glm::vec3 max);

    static void init();
    static void cleanup();
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
    static constexpr std::uint32_t MESH_STAGING_BUFFER_SIZE{256 * 1024 * 1024};  // 256 MB

    static std::uint32_t sLatestMeshId;
    std::uint32_t mAssetid;
    std::uint32_t mId;
    std::string mName;
    std::vector<SwPrimitive> mPrimitives;
    SwBounds mBounds;

public:
    static SwStagingBuffer sMeshStaging;

    std::uint32_t mRelativeFirstBounds;
    SwAllocatedBuffer mVertexBuffer;
    std::uint32_t mNumVertices{0};
    std::uint32_t mVertexOffsetInScene{0};
    SwAllocatedBuffer mIndexBuffer;
    std::uint32_t mNumIndices{0};
    std::uint32_t mFirstIndexInScene{0};

    SwMesh(
        std::uint32_t assetId, std::string name, std::vector<SwPrimitive> primitives, SwBounds bounds, std::uint32_t relativeFirstBounds,
        SwAllocatedBuffer vertexBuffer,
        std::uint32_t numVertices, std::uint32_t vertexOffsetInScene, SwAllocatedBuffer indexBuffer, std::uint32_t numIndices, std::uint32_t firstIndexInScene
    );

    inline SwAllocatedBuffer& getVertexBuffer() { return mVertexBuffer; }
    inline SwAllocatedBuffer& getIndexBuffer() { return mIndexBuffer; }
    inline SwBounds getBounds() const { return mBounds; }
    inline std::span<SwPrimitive> getPrimitives() { return mPrimitives; }
    inline std::uint32_t getAssetId() const { return mAssetid; }

    static void init();
    static void cleanup();
};