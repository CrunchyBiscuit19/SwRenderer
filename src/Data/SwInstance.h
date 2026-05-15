#pragma once

#include <glm/glm.hpp>

class SwModel;

struct SwInstanceData {
    glm::mat4 mTransformMatrix;

    SwInstanceData() : mTransformMatrix(glm::mat4(1.f)) {}

    SwInstanceData(glm::mat4&& transform) : mTransformMatrix(transform) {}
};

class SwInstance {
private:
    SwModel& mModel;
    std::uint32_t mId;
    bool mDelete;

    SwInstanceData mData;

    SwInstance(SwModel& model, std::uint32_t id, SwInstanceData data = SwInstanceData());
};