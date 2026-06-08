#pragma once

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
    static constexpr std::uint32_t NODE_TRANSFORMS_STAGING_BUFFER_SIZE{1 << 22};  // 4 MB


    std::string mName;
    std::uint32_t mRelativeNodeIndex;
    std::weak_ptr<SwNode> mParent;
    std::vector<std::shared_ptr<SwNode>> mChildren;
    glm::mat4 mLocalTransform;
    glm::mat4 mWorldTransform;

public:
    static SwStagingBuffer sNodeTransformsStaging;

    SwNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform);

    inline std::weak_ptr<SwNode> getParent() { return mParent; }

    inline glm::mat4& getWorldTransform() { return mWorldTransform; }

    void setParent(std::weak_ptr<SwNode> parent);

    void addChild(std::shared_ptr<SwNode> child);

    void refreshTransform(const glm::mat4& parentTransform);

    virtual void generateRcsAndRis();

    static void init();
    static void cleanup();
};

class SwMeshNode : public SwNode {
    SwMesh& mMesh;

public:
    SwMeshNode(std::string name, std::uint32_t relativeNodeIndex, glm::mat4 localTransform, SwMesh& mesh);

    void generateRcsAndRis() override;
};