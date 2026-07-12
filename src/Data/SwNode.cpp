#include <Data/SwBatch.h>
#include <Data/SwMesh.h>
#include <Data/SwNode.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwRendererContext.h>
#include <Scene/SwScene.h>

void SwNode::refreshTransform(const glm::mat4& parentTransform) {
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& child : mChildren) child->refreshTransform(mWorldTransform);
}

void SwNode::generateRcsAndRis(SwAsset& asset) {
    for (const auto& child : mChildren) child->generateRcsAndRis(asset);
}

SwNode::SwNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform)
    : mName(name), mRelativeNodeIndex(relativeNodeIndex), mLocalTransform(localTransform) {}

void SwNode::setParent(std::weak_ptr<SwNode> parent) { mParent = parent; }

void SwNode::addChild(std::shared_ptr<SwNode> child) {
    mChildren.emplace_back(child);
    child->setParent(shared_from_this());
}

SwMeshNode::SwMeshNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, std::uint32_t meshIndex)
    : SwNode(name, relativeNodeIndex, localTransform), mMeshIndex(meshIndex) {}

void SwMeshNode::generateRcsAndRis(SwAsset& asset) {
    SwMesh& mesh = asset.getMeshes()[mMeshIndex];
    for (auto& primitive : mesh.getPrimitives()) {
        SwMaterial& material = asset.getMaterials()[primitive.mMaterialIndex];

        // mFirstRi assigned during the scene-wide flatten in SwScene::regenerateRcsAndRis.
        SwRenderCommand rc{
            primitive.mIndexCount,
            0,
            mesh.mFirstIndexInScene + primitive.mRelativeFirstIndex,
            mesh.mVertexOffsetInScene + primitive.mRelativeVertexOffset,
            0,
            asset.mFirstMaterialInScene + material.mRelativeMaterialIndex,
            asset.mFirstNodeTransformInScene + this->mRelativeNodeIndex,
            asset.getId(),
            asset.mFirstInstanceInScene,
            asset.mFirstBoundInScene + mesh.mRelativeFirstBounds,
            0
        };

        SwRenderer::sRendererContext.mScene->recordPendingDraw(material, rc, static_cast<std::uint32_t>(asset.getInstanceIds().size()));
    }

    SwNode::generateRcsAndRis(asset);
}

SwLightNode::SwLightNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, std::uint32_t lightIndex, std::uint32_t assetId)
    : SwNode(name, relativeNodeIndex, localTransform), mLightIndex(lightIndex), mAssetId(assetId) {}
