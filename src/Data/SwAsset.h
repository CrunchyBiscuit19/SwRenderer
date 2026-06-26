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
    bool mStandaloneLight{false};  

    fastgltf::Asset mRawAsset;

    struct DecodedImage {
        unsigned char* mData{nullptr};
        std::int32_t mWidth{0};
        std::int32_t mHeight{0};
        std::string mError;  
    };

    std::vector<SwMesh> mMeshes;

    std::vector<SwLight> mLights;

    static std::unordered_map<SwSamplerOptions, SwSampler> sSamplers;
    std::vector<SwSamplerOptions> mSamplerOptions;
    
    std::vector<std::optional<SwColorImage2D>> mImages;

    std::vector<SwMaterial> mMaterials;
    SwAllocatedBuffer mMaterialConstantsBuffer;
    
    std::vector<std::shared_ptr<SwNode>> mTopNodes;
    std::vector<std::shared_ptr<SwNode>> mNodes;
    SwAllocatedBuffer mNodeTransformsBuffer;

    std::deque<SwInstance> mInstances;
    SwAllocatedBuffer mInstancesBuffer;

    std::uint32_t mNumBounds;
    SwAllocatedBuffer mBoundsBuffer;

    static vk::Filter extractFilter(fastgltf::Filter filter);
    static vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
    static vk::SamplerAddressMode extractAddressMode(fastgltf::Wrap wrap);
    static SwLight::Type mapLightType(fastgltf::LightType type);

    void loadRawAsset(std::filesystem::path& assetPath);
    void constructBuffers();
    void constructSamplerAndSamplerOptions();
    DecodedImage decodeImage(std::uint32_t imageIndex);
    void constructImages();
    void constructMaterials();
    void constructMeshes();
    void constructLights();
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

    void generateRcsAndRis();

    void createInstance(SwInstance::Data instanceData = SwInstance::Data());
    void createInstance(SwCamera& camera);

    void reloadInstances();

    void markDelete();

    inline void setReloadInstancesFlag(bool flag) { mReloadInstancesFlag = flag; }
    inline void setStandaloneLight(bool flag) { mStandaloneLight = flag; }
    inline bool isStandaloneLight() { return mStandaloneLight; }
    inline std::string getName() { return mName; }
    inline std::uint32_t getId() { return mId; }
    inline std::deque<SwInstance>& getInstances() { return mInstances; }
    inline bool isMarkedDelete() { return mDelete; }
    inline bool getReloadInstancesFlag() { return mReloadInstancesFlag; }
    inline std::span<SwMesh> getMeshes() { return mMeshes; }
    inline std::span<SwMaterial> getMaterials() { return mMaterials; }
    inline std::span<SwLight> getLights() { return mLights; }
    inline std::span<std::shared_ptr<SwNode>> getNodes() { return mNodes; }
    inline SwAllocatedBuffer& getMaterialConstantsBuffer() { return mMaterialConstantsBuffer; }
    inline SwAllocatedBuffer& getNodeTransformsBuffer() { return mNodeTransformsBuffer; }
    inline SwAllocatedBuffer& getInstancesBuffer() { return mInstancesBuffer; }
    inline SwAllocatedBuffer& getBoundsBuffer() { return mBoundsBuffer; }
};