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

    std::uint32_t mAssetId;
    std::uint32_t mId;
    bool mDelete;

    SwInstanceData mData;

public:
    SwInstance(std::uint32_t assetId, SwInstanceData data = SwInstanceData());

    inline std::uint32_t getId() const { return mId; }
    inline SwInstanceData* getDataAddress() { return &mData; }
    inline std::uint32_t getAssetId() const { return mAssetId; }
    inline bool isMarkedDelete() const { return mDelete; }

    inline void markDelete() { mDelete = true; }
};