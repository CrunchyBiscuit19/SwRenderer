#pragma once

#include <Data/SwInstance.h>
#include <Data/SwMesh.h>
#include <Data/SwNode.h>
#include <Renderer/SwRendererContext.h>

#include <fastgltf/parser.hpp>
#include <string>

class SwAsset {
private:
    static constexpr std::uint32_t NUM_ASSET_INSTANCES{1 << 6};
    static constexpr std::uint32_t NUM_ASSET_MATERIALS{1 << 6};
    static constexpr std::uint32_t NUM_ASSET_NODES{1 << 10};
    static constexpr std::uint32_t NUM_ASSET_BOUNDS{1 << 10};

    static std::uint32_t sLatestAssetId;

    std::string mName;
    std::uint32_t mId{0};
    bool mDelete{false};
    bool mReloadInstancesFlag{true};

    fastgltf::Asset mRawAsset;

    std::vector<SwMesh> mMeshes;
    static std::unordered_map<SwSamplerOptions, SwSampler> sSamplers;
    std::vector<SwSamplerOptions> mSamplerOptions;
    std::vector<std::optional<SwColorImage2D>> mImages;
    std::vector<bool> mImageCreated;
    std::vector<SwMaterial> mMaterials;
    SwAllocatedBuffer mMaterialConstantsBuffer;

    std::vector<std::shared_ptr<SwNode>> mTopNodes;
    std::vector<std::shared_ptr<SwNode>> mNodes;
    SwAllocatedBuffer mNodeTransformsBuffer;

    std::vector<SwInstance> mInstances;
    SwAllocatedBuffer mInstancesBuffer;

    std::uint32_t mNumBounds;
    SwAllocatedBuffer mBoundsBuffer;

    static vk::Filter extractFilter(fastgltf::Filter filter);
    static vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
    static vk::SamplerAddressMode extractAddressMode(fastgltf::Wrap wrap);

    void loadRawAsset(std::filesystem::path& assetPath);
    void constructBuffers();
    void constructSamplerAndSamplerOptions();
    void constructImage(std::uint32_t imageIndex, SwMaterialTexture::Type texType);
    void constructMaterials();
    void constructMeshes();
    void constructNodes();

public:
    std::uint32_t mFirstMaterialInScene{0};
    std::uint32_t mFirstNodeTransformInScene{0};
    std::uint32_t mFirstInstanceInScene{0};
    std::uint32_t mFirstBoundInScene{0};

    static void init();

    static void cleanup();

    static std::string getNameFromFilePath(const std::filesystem::path& assetPath);

    SwAsset() = default;
    SwAsset(std::filesystem::path& assetPath);

    void generateRItemsAndRInsts();

    void createInstance(SwInstance::Data instanceData = SwInstance::Data());
    void createInstance(SwCamera& camera);

    void reloadInstances();

    void markDelete();

    inline void setReloadInstancesFlag(bool flag) { mReloadInstancesFlag = flag; }
    inline std::string getName() { return mName; }
    inline std::uint32_t getId() { return mId; }
    inline std::vector<SwInstance>& getInstances() { return mInstances; }
    inline bool isMarkedDelete() { return mDelete; }
    inline bool getReloadInstancesFlag() { return mReloadInstancesFlag; }
    inline std::span<SwMesh> getMeshes() { return mMeshes; }
    inline std::span<SwMaterial> getMaterials() { return mMaterials; }
    inline std::span<std::shared_ptr<SwNode>> getNodes() { return mNodes; }
    inline SwAllocatedBuffer& getMaterialConstantsBuffer() { return mMaterialConstantsBuffer; }
    inline SwAllocatedBuffer& getNodeTransformsBuffer() { return mNodeTransformsBuffer; }
    inline SwAllocatedBuffer& getInstancesBuffer() { return mInstancesBuffer; }
    inline SwAllocatedBuffer& getBoundsBuffer() { return mBoundsBuffer; }
};