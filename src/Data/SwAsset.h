#pragma once

#include <Data/SwMesh.h>
#include <Data/SwNode.h>
#include <Data/SwInstance.h>
#include <Renderer/SwRendererContext.h>

#include <fastgltf/parser.hpp>

#include <string>

class SwAsset {
private:
    static const std::uint32_t NUM_MODEL_INSTANCES = 1 << 7;
    static const std::uint32_t NUM_MODEL_MATERIALS = 1 << 7;
    static const std::uint32_t NUM_MODEL_NODES = 1 << 13;
    static const std::uint32_t NUM_MODEL_BOUNDS = 1 << 13;

    static std::uint32_t sLatestAssetId;
    static SwAssetContext sAssetContext;

    std::string mName;
    std::uint32_t mId{0};
    bool mDelete{false};
    bool mReloadInstancesFlag{true};

    fastgltf::Asset mRawAsset;

    std::vector<SwMesh> mMeshes;
    static std::unordered_map<SwSamplerOptions, SwSampler> sSamplers;
    std::vector<SwSamplerOptions> mSamplerOptions;
    std::vector<SwColorImage2D> mImages;
    std::vector<SwMaterial> mMaterials;
    SwAllocatedBuffer mMaterialConstantsBuffer;
    std::uint32_t mFirstMaterialInScene{0};

    std::vector<std::shared_ptr<SwNode>> mTopNodes;
    std::vector<std::shared_ptr<SwNode>> mNodes;
    SwAllocatedBuffer mNodeTransformsBuffer;
    std::uint32_t mFirstNodeTransformInScene{0};

    std::vector<SwInstance> mInstances;
    SwAllocatedBuffer mInstancesBuffer;
    std::uint32_t mFirstInstanceInScene{0};

    std::uint32_t mNumBounds;
    SwAllocatedBuffer mBoundsBuffer;
    std::uint32_t mFirstBoundInScene{0};

    static vk::Filter extractFilter(fastgltf::Filter filter);
    static vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
    static vk::SamplerAddressMode extractAddressMode(fastgltf::Wrap wrap);

    void loadRawAsset(std::filesystem::path& assetPath);
    void constructBuffers();
    void constructSamplers();
    void constructImages();
    void constructMaterials();
    void constructMeshes();
    void constructNodes();  

public:
    static void init(SwAssetContext assetContext);

    static void cleanup();

    SwAsset(std::filesystem::path& assetPath);

    void generateRenderItemsAndRenderInstances();

    void createInstance(SwInstanceData instanceData = SwInstanceData());
    
    void reloadInstances();

    void markDelete();
};