#pragma once

#include <glm/glm.hpp>

struct SwInstanceData {
    glm::mat4 mTransformMatrix;

    SwInstanceData();
    SwInstanceData(glm::mat4&& transform);
};

class SwInstance {
private:
    static uint32_t sLatestInstanceId;

    std::string mAssetName;
    std::uint32_t mId;
    bool mDelete;

    SwInstanceData mData;

public:
    SwInstance(std::string assetName, SwInstanceData data = SwInstanceData());

    inline void* getDataAddress() { return &mData; }

    inline void markDelete() { mDelete = true; }
};