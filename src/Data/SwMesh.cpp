#include <Data/SwAsset.h>
#include <Data/SwMesh.h>

SwVertex::SwVertex(glm::vec3 position, glm::vec3 normal, glm::vec4 color, glm::vec2 uv) : mPosition(position), mNormal(normal), mColor(color), mUv(uv) {}

SwBounds::SwBounds(glm::vec3 min, glm::vec3 max) : mMin(min), mMax(max) {}

SwPrimitive::SwPrimitive(std::uint32_t relativeFirstIndex, std::uint32_t indexCount, std::uint32_t relativeVertexOffset, std::uint32_t materialIndex)
    : mRelativeFirstIndex(relativeFirstIndex), mIndexCount(indexCount), mRelativeVertexOffset(relativeVertexOffset), mMaterialIndex(materialIndex) {}

std::uint32_t SwMesh::sLatestMeshId{0};

SwMesh::SwMesh() : mId(sLatestMeshId++) {}

SwMesh::SwMesh(
    std::uint32_t assetId, std::string name, std::vector<SwPrimitive> primitives, std::vector<SwVertex> vertices, std::vector<std::uint32_t> indices,
    SwBounds bounds, std::uint32_t relativeFirstBounds, std::uint32_t numVertices, std::uint32_t vertexOffsetInAsset, std::uint32_t vertexOffsetInScene,
    std::uint32_t numIndices, std::uint32_t firstIndexInAsset, std::uint32_t firstIndexInScene
)
    : mId(sLatestMeshId++),
      mAssetId(assetId),
      mName(name),
      mPrimitives(std::move(primitives)),
      mVertices(std::move(vertices)),
      mIndices(std::move(indices)),
      mBounds(bounds),
      mRelativeFirstBounds(relativeFirstBounds),
      mNumVertices(numVertices),
      mVertexOffsetInAsset(vertexOffsetInAsset),
      mVertexOffsetInScene(vertexOffsetInScene),
      mNumIndices(numIndices),
      mFirstIndexInAsset(firstIndexInAsset),
      mFirstIndexInScene(firstIndexInScene) {}
