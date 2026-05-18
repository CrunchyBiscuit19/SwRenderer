#include <Data/SwInstance.h>

SwInstanceData::SwInstanceData() : mTransformMatrix(glm::mat4(1.f)) {};

SwInstanceData::SwInstanceData(glm::mat4&& transform) : mTransformMatrix(transform) {};

uint32_t SwInstance::sLatestInstanceId{0};

SwInstance::SwInstance(std::string assetName, SwInstanceData data) : mAssetName(assetName), mId(sLatestInstanceId++), mData(data) {}
