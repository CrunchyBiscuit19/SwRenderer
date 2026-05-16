#include "SwMesh.h"

SwVertex::SwVertex(glm::vec3 position, glm::vec3 normal, glm::vec4 color, glm::vec2 uv) : mPosition(position), mNormal(normal), mColor(color), mUv(uv) {}

SwBounds::SwBounds(glm::vec3 min, glm::vec3 max) : mMin(min), mMax(max) {}

SwPrimitive::SwPrimitive(std::uint32_t relativeFirstIndex, std::uint32_t indexCount, std::uint32_t relativeVertexOffset, SwMaterial& material)
    : mRelativeFirstIndex(relativeFirstIndex), mIndexCount(indexCount), mRelativeVertexOffset(relativeVertexOffset), mMaterial(material) {}

SwMesh::SwMesh(fastgltf::Mesh& mesh) {}
