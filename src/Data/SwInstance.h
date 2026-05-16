#pragma once

#include <glm/glm.hpp>

#include <filesystem>

struct SwInstanceData {
    glm::mat4 mTransformMatrix;

    SwInstanceData();
    SwInstanceData(glm::mat4&& transform);
};

class SwInstance {
private:
    static uint32_t sLatestInstanceId;

    std::filesystem::path& mAssetName;
    std::uint32_t mId;
    bool mDelete;

    SwInstanceData mData;

    SwInstance(std::filesystem::path& assetName, std::uint32_t id, SwInstanceData data = SwInstanceData());
};