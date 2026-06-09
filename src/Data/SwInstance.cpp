#include <Data/SwInstance.h>
#include <Renderer/SwRenderer.h>

uint32_t SwInstance::sLatestInstanceId{0};

SwInstance::SwInstance(std::uint32_t assetId, SwInstance::Data data) : mAssetId(assetId), mId(sLatestInstanceId++), mData(data) {}

