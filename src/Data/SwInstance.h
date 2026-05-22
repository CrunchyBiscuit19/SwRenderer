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

    inline SwInstanceData* getDataAddress() { return &mData; }
    inline std::string getAssetName() const { return mAssetName; }

    inline void markDelete() { mDelete = true; }
};