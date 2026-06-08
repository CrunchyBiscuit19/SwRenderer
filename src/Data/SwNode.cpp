#include <Data/SwBatch.h>
#include <Renderer/SwRenderer.h>
#include <Data/SwMesh.h>
#include <Data/SwNode.h>
#include <Renderer/SwRendererContext.h>
#include <Scene/SwScene.h>

SwStagingBuffer SwNode::sNodeTransformsStaging{};

void SwNode::refreshTransform(const glm::mat4& parentTransform) {
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& child : mChildren) child->refreshTransform(mWorldTransform);
}

void SwNode::generateRcsAndRis() {
    for (const auto& child : mChildren) child->generateRcsAndRis();
}

SwNode::SwNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform)
    : mName(name), mRelativeNodeIndex(relativeNodeIndex), mLocalTransform(localTransform) {}

void SwNode::setParent(std::weak_ptr<SwNode> parent) { mParent = parent; }

void SwNode::addChild(std::shared_ptr<SwNode> child) {
    mChildren.emplace_back(child);
    child->setParent(shared_from_this());
}

void SwNode::init() {
    sNodeTransformsStaging = SwBufferFactory::createStagingBuffer(NODE_TRANSFORMS_STAGING_BUFFER_SIZE);
}

void SwNode::cleanup() { sNodeTransformsStaging.destroy(); }

SwMeshNode::SwMeshNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, SwMesh& mesh)
    : SwNode(name, relativeNodeIndex, localTransform), mMesh(mesh) {}

void SwMeshNode::generateRcsAndRis() {
    for (auto& primitive : mMesh.getPrimitives()) {
        std::uint32_t pipelineId = primitive.mMaterial.getPipelineBundle().getID();

        SwAsset& workingAsset = SwRenderer::sRendererContext.mScene->getAsset(mMesh.getAssetId());
        std::unordered_map<std::uint32_t, SwBatch>& workingBatchMap =
            SwRenderer::sRendererContext.mScene->getBatchMap(SwMaterial::getMaterialTypeFromAlphaMode(primitive.mMaterial.getAlphaMode()));

        auto [it, inserted] = workingBatchMap.try_emplace(pipelineId, primitive);
        SwBatch& workingBatch = it->second;
        workingBatch.getRcs().emplace_back(
            primitive.mIndexCount,
            0,  // Instance count set to 0, incremented inside culling compute shader
            mMesh.mFirstIndexInScene + primitive.mRelativeFirstIndex,
            mMesh.mVertexOffsetInScene + primitive.mRelativeVertexOffset,
            SwBatch::sFirstRiOffset,
            workingAsset.mFirstMaterialInScene + primitive.mMaterial.mRelativeMaterialIndex,
            workingAsset.mFirstNodeTransformInScene + this->mRelativeNodeIndex,
            workingAsset.getId(),
            workingAsset.mFirstInstanceInScene,
            workingAsset.mFirstBoundInScene + mMesh.mRelativeFirstBounds
        );

        std::uint32_t rcIndex = static_cast<std::uint32_t>(workingBatch.getRcs().size() - 1);
        std::uint32_t instanceIndex = workingAsset.mFirstInstanceInScene;
        for (std::uint32_t i = 0; i < workingAsset.getInstances().size(); i++) {
            workingBatch.getRis().emplace_back(rcIndex, instanceIndex + i);
        }

        SwBatch::sFirstRiOffset += workingAsset.getInstances().size();
    }

    SwNode::generateRcsAndRis();
}
