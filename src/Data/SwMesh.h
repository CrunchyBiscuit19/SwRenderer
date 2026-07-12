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
    glm::vec3 mMin{0.f};
    glm::vec3 mMax{0.f};

    SwBounds() = default;
    SwBounds(glm::vec3 min, glm::vec3 max);
};

struct SwPrimitive {
    std::uint32_t mRelativeFirstIndex{0};
    std::uint32_t mIndexCount{0};
    std::uint32_t mRelativeVertexOffset{0};
    std::uint32_t mMaterialIndex{0};

    SwPrimitive(std::uint32_t relativeFirstIndex, std::uint32_t indexCount, std::uint32_t relativeVertexOffset, std::uint32_t materialIndex);
};

class SwMesh {
private:
    static std::uint32_t sLatestMeshId;
    std::uint32_t mId{0};
    std::uint32_t mAssetId{0};
    std::string mName;
    std::vector<SwPrimitive> mPrimitives;
    std::vector<SwVertex> mVertices;
    std::vector<std::uint32_t> mIndices;
    SwBounds mBounds;

public:
    std::uint32_t mRelativeFirstBounds{0};
    std::uint32_t mNumVertices{0};
    std::uint32_t mVertexOffsetInAsset{0};
    std::uint32_t mVertexOffsetInScene{0};
    std::uint32_t mNumIndices{0};
    std::uint32_t mFirstIndexInAsset{0};
    std::uint32_t mFirstIndexInScene{0};

    SwMesh();
    SwMesh(
        std::uint32_t assetId, std::string name, std::vector<SwPrimitive> primitives, std::vector<SwVertex> vertices, std::vector<std::uint32_t> indices,
        SwBounds bounds, std::uint32_t relativeFirstBounds, std::uint32_t numVertices, std::uint32_t vertexOffsetInAsset, std::uint32_t vertexOffsetInScene,
        std::uint32_t numIndices, std::uint32_t firstIndexInAsset, std::uint32_t firstIndexInScene
    );

    inline void setAssetId(std::uint32_t assetId) { mAssetId = assetId; }
    inline void setName(std::string name) { mName = name; }
    inline std::vector<SwPrimitive>& getPrimitives() { return mPrimitives; }
    inline std::vector<SwVertex>& getVertices() { return mVertices; }
    inline std::vector<std::uint32_t>& getIndices() { return mIndices; }
    inline void setBounds(glm::vec3 min, glm::vec3 max) { mBounds = {min, max}; }
    inline SwBounds getBounds() const { return mBounds; }
    inline std::uint32_t getAssetId() const { return mAssetId; }
};