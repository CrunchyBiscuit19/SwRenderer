#include <Data/SwNode.h>
#include <Data/SwMesh.h>
#include <Data/SwBatch.h>

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

void SwNode::init() { sNodeTransformsStagingBuffer = SwBufferFactory::createStagingBuffer(NODE_TRANSFORMS_STAGING_BUFFER_SIZE); }

void SwNode::cleanup() { sNodeTransformsStagingBuffer.destroy(); }

SwMeshNode::SwMeshNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, SwMesh& mesh)
    : SwNode(name, relativeNodeIndex, localTransform), mMesh(mesh) {}

void SwMeshNode::generateRenderItemsAndRenderInstances() {
    for (auto& primitive : mMesh.getPrimitives()) {

        std::uint32_t pipelineId = primitive.mMaterial.getPipelineBundle().getID();

        /* // TODO after scene implemented
        // Select which batch type to use based on the material's alpha mode
        switch (primitive.mMaterial.getAlphaMode()) {
            case fastgltf::AlphaMode::Opaque:
                
            case fastgltf::AlphaMode::Mask:
                
            case fastgltf::AlphaMode::Blend:
                
        }

        renderer->mScene.mBatchTypes[batchType]->try_emplace(pipelineId, renderer, primitive, pipelineId);
        renderer->mScene.mBatchTypes[batchType]
            ->at(pipelineId)
            .renderItems.emplace_back(
                primitive.mIndexCount,
                0,  // Instance count set to 0, incremented inside culling compute shader
                mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
                mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset,
                SwBatch::sFirstRenderInstanceOffset,
                model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
                model->mMainFirstNodeTransform + this->mRelativeNodeIndex,
                model->mId,
                model->mMainFirstInstance,
                model->mMainFirstBounds + mMesh->mRelativeFirstBounds
            );


        SwRenderItem& currRenderItem = renderer->mScene.mBatchTypes[batchType]->at(pipelineId).renderItems.back();
        std::uint32_t renderItemIndex = static_cast<std::uint32_t>(renderer->mScene.mBatchTypes[batchType]->at(pipelineId).renderItems.size() - 1);
        std::uint32_t instanceIndex = model->mMainFirstInstance;
        for (std::uint32_t i = 0; i < model->mInstances.size(); i++) {
            renderer->mScene.mBatchTypes[batchType]->at(pipelineId).renderInstances.emplace_back(renderItemIndex, instanceIndex + i);
        }
        SwBatch::sFirstRenderInstanceOffset += model->mInstances.size();
        */
    }

    SwNode::generateRenderItemsAndRenderInstances();
}
