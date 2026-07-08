#pragma once

#include <Data/SwLight.h>
#include <Resource/SwBuffer.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <string>
#include <vector>

class SwMesh;
class SwAsset;

class SwNode : public std::enable_shared_from_this<SwNode> {
protected:
    std::string mName;
    std::uint32_t mRelativeNodeIndex;
    std::weak_ptr<SwNode> mParent;
    std::vector<std::shared_ptr<SwNode>> mChildren;
    glm::mat4 mLocalTransform;
    glm::mat4 mWorldTransform;

public:
    SwNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform);

    inline std::weak_ptr<SwNode> getParent() { return mParent; }

    inline std::uint32_t getRelativeNodeIndex() const { return mRelativeNodeIndex; }

    inline glm::mat4& getWorldTransform() { return mWorldTransform; }

    void setParent(std::weak_ptr<SwNode> parent);

    void addChild(std::shared_ptr<SwNode> child);

    void refreshTransform(const glm::mat4& parentTransform);

    virtual void generateRcsAndRis(SwAsset& asset);
};

class SwMeshNode : public SwNode {
    std::uint32_t mMeshIndex;

public:
    SwMeshNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, std::uint32_t meshIndex);

    void generateRcsAndRis(SwAsset& asset) override;
};

class SwLightNode : public SwNode {
    std::uint32_t mLightIndex;
    std::uint32_t mAssetId;

public:
    SwLightNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, std::uint32_t lightIndex, std::uint32_t assetId);

    inline std::uint32_t getLightIndex() const { return mLightIndex; }
};