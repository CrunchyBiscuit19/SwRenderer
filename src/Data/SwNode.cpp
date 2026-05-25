#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <Data/SwNode.h>
#include <Renderer/SwRendererContext.h>
#include <Scene/SwScene.h>

SwRendererContext SwNode::sRendererContext{};
SwStagingBuffer SwNode::sNodeTransformsStagingBuffer{};

void SwNode::refreshTransform(const glm::mat4& parentTransform) {
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& child : mChildren) child->refreshTransform(mWorldTransform);
}

void SwNode::generateRenderItemsAndRenderInstances() {
    for (const auto& child : mChildren) child->generateRenderItemsAndRenderInstances();
}

SwNode::SwNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform)
    : mName(name), mRelativeNodeIndex(relativeNodeIndex), mLocalTransform(localTransform) {}

void SwNode::setParent(std::weak_ptr<SwNode> parent) { mParent = parent; }

void SwNode::addChild(std::shared_ptr<SwNode> child) {
    mChildren.emplace_back(child);
    child->setParent(shared_from_this());
}

void SwNode::init(SwRendererContext rendererContext) {
    sRendererContext = rendererContext;
    sNodeTransformsStagingBuffer = SwBufferFactory::createStagingBuffer(NODE_TRANSFORMS_STAGING_BUFFER_SIZE);
}

void SwNode::cleanup() { sNodeTransformsStagingBuffer.destroy(); }

SwMeshNode::SwMeshNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, SwMesh& mesh)
    : SwNode(name, relativeNodeIndex, localTransform), mMesh(mesh) {}

void SwMeshNode::generateRenderItemsAndRenderInstances() {
    for (auto& primitive : mMesh.getPrimitives()) {
        std::uint32_t pipelineId = primitive.mMaterial.getPipelineBundle().getID();

        SwAsset& workingAsset = sRendererContext.mScene->getAsset(mMesh.getAssetId());
        std::unordered_map<std::uint32_t, SwBatch>& workingBatchMap =
            sRendererContext.mScene->getBatchesByType(SwMaterial::getMaterialTypeFromAlphaMode(primitive.mMaterial.getAlphaMode()));

        auto [it, inserted] = workingBatchMap.try_emplace(pipelineId, primitive);
        SwBatch& workingBatch = it->second;
        workingBatch.getRenderItems().emplace_back(
            primitive.mIndexCount,
            0,  // Instance count set to 0, incremented inside culling compute shader
            mMesh.mFirstIndexInScene + primitive.mRelativeFirstIndex,
            mMesh.mVertexOffsetInScene + primitive.mRelativeVertexOffset,
            SwBatch::sFirstRenderInstanceOffset,
            workingAsset.mFirstMaterialInScene + primitive.mMaterial.mRelativeMaterialIndex,
            workingAsset.mFirstNodeTransformInScene + this->mRelativeNodeIndex,
            workingAsset.getId(),
            workingAsset.mFirstInstanceInScene,
            workingAsset.mFirstBoundInScene + mMesh.mRelativeFirstBounds
        );

        std::uint32_t renderItemIndex = static_cast<std::uint32_t>(workingBatch.getRenderItems().size() - 1);
        std::uint32_t instanceIndex = workingAsset.mFirstInstanceInScene;
        for (std::uint32_t i = 0; i < workingAsset.getInstances().size(); i++) {
            workingBatch.getRenderInstances().emplace_back(renderItemIndex, instanceIndex + i);
        }
        SwBatch::sFirstRenderInstanceOffset += workingAsset.getInstances().size();
    }

    SwNode::generateRenderItemsAndRenderInstances();
}
