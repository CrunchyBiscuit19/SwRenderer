#include <Data/SwMesh.h>
#include <Data/SwAsset.h>
#include <fmt/core.h>

SwVertex::SwVertex(glm::vec3 position, glm::vec3 normal, glm::vec4 color, glm::vec2 uv) : mPosition(position), mNormal(normal), mColor(color), mUv(uv) {}

SwStagingBuffer SwBounds::sBoundsStaging{};

SwBounds::SwBounds(glm::vec3 min, glm::vec3 max) : mMin(min), mMax(max) {}

void SwBounds::init() { sBoundsStaging = SwBufferFactory::createStagingBuffer("BoundsStagingBuffer", BOUNDS_STAGING_BUFFER_SIZE); }

void SwBounds::cleanup() { sBoundsStaging.destroy(); }

SwPrimitive::SwPrimitive(std::uint32_t relativeFirstIndex, std::uint32_t indexCount, std::uint32_t relativeVertexOffset, SwMaterial& material)
    : mRelativeFirstIndex(relativeFirstIndex), mIndexCount(indexCount), mRelativeVertexOffset(relativeVertexOffset), mMaterial(material) {}

SwStagingBuffer SwMesh::sMeshStaging{};
std::uint32_t SwMesh::sLatestMeshId{0};

SwMesh::SwMesh(
    std::uint32_t assetId, std::string name, std::vector<SwPrimitive> primitives, SwBounds bounds, std::uint32_t relativeFirstBounds,
    SwAllocatedBuffer vertexBuffer,
    std::uint32_t numVertices, std::uint32_t vertexOffsetInScene, SwAllocatedBuffer indexBuffer, std::uint32_t numIndices, std::uint32_t firstIndexInScene
)
    : mId(sLatestMeshId++),
      mAssetid(assetId),
      mName(name),
      mPrimitives(primitives),
      mBounds(bounds),
      mRelativeFirstBounds(relativeFirstBounds),
      mVertexBuffer(std::move(vertexBuffer)),
      mNumVertices(numVertices),
      mVertexOffsetInScene(vertexOffsetInScene),
      mIndexBuffer(std::move(indexBuffer)),
      mNumIndices(numIndices),
      mFirstIndexInScene(firstIndexInScene) {
}

void SwMesh::init() { sMeshStaging = SwBufferFactory::createStagingBuffer("MeshStagingBuffer", MESH_STAGING_BUFFER_SIZE); }

void SwMesh::cleanup() { sMeshStaging.destroy(); }
