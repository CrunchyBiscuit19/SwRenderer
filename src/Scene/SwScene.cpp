
#include <Renderer/SwEvents.h>
#include <Renderer/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwStagingRing.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <quill/LogMacros.h>
#include <stb_image.h>

#include <format>
#include <glm/glm.hpp>
#include <optional>
#include <ranges>

void SwScene::initializeMiscPasses() {
    // Clear Images
    mPasses[SwPass::Type::ClearImages] = SwPass(SwPass::Type::ClearImages, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 4> colorAttachments = {
            SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mWBOIT.getResources().mAccumImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mWBOIT.getResources().mRvlImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mPick.getResources().mReadbackImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
        };
        vk::RenderingAttachmentInfo depthAttachment =
            SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
        vk::RenderingInfo renderInfo =
            SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colorAttachments, depthAttachment);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();
    });

    // Copy to Swapchain
    mPasses[SwPass::Type::CopyToSwapchain] = SwPass(SwPass::Type::CopyToSwapchain, [&](vk::CommandBuffer cmd) {
        SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().copyFrom(cmd, SwRenderer::sRendererContext.mSwapchain->getDrawImage());
    });
}

void SwScene::initializeResources() {
    mVertexBuffer =
        SwBufferFactory::createAllocatedBuffer("VertexBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mIndexBuffer = SwBufferFactory::createAllocatedBuffer("IndexBuffer", vk::BufferUsageFlagBits::eIndexBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE);
    mMaterialConstantsBuffer =
        SwBufferFactory::createAllocatedBuffer("MaterialConstantsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, MATERIAL_CONSTANTS_BUFFER_SIZE, true);
    mNodeTransformsBuffer =
        SwBufferFactory::createAllocatedBuffer("NodeTransformsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mInstancesBuffer =
        SwBufferFactory::createAllocatedBuffer("InstancesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mBoundsBuffer =
        SwBufferFactory::createAllocatedBuffer("BoundsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mRisIndicesBuffer =
        SwBufferFactory::createAllocatedBuffer("RisIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    for (std::uint32_t i = 0; i < mVisibilityRisBuffers.size(); i++) {
        mVisibilityRisBuffers[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("VisibilityRisBuffer{}", i), vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
        );
    }
    mInitialRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        "InitialRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mEarlyRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        "EarlyRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mEarlyRcsCount = SwBufferFactory::createAllocatedBuffer(
        "EarlyRcsCount", vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mLateRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        "LateRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mLateRcsCount = SwBufferFactory::createAllocatedBuffer(
        "LateRcsCount", vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mRisBuffer = SwBufferFactory::createAllocatedBuffer("RisBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mBatchesBuffer =
        SwBufferFactory::createAllocatedBuffer("BatchesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);

    mMaterialSamplersDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        "MaterialSamplersDescriptorSet", SwMaterialResources::sMaterialSamplersDescriptorLayout, NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES
    );
    mMaterialTexturesDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        "MaterialTexturesDescriptorSet", SwMaterialResources::sMaterialTexturesDescriptorLayout, NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES
    );
    mLightsBuffer =
        SwBufferFactory::createAllocatedBuffer("LightsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);

    constexpr std::uint32_t normalSlot = static_cast<std::uint32_t>(SwMaterialTexture::Type::Normal);
    for (std::uint32_t i = 0; i < NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES; i++) {
        SwMaterialTexture& seed =
            (i % SwMaterial::NUM_PBR_IMAGES == normalSlot) ? SwMaterialTexture::sDefaultFlatNormalTexture : SwMaterialTexture::sDefaultWhiteTexture;
        mMaterialSamplersDescriptorSet.writeSampler(0, seed.getSampler().getHandle(), i);
        mMaterialTexturesDescriptorSet.writeImage(0, seed.getImage().getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i);
    }
    mMaterialSamplersDescriptorSet.pushWrites();
    mMaterialTexturesDescriptorSet.pushWrites();
}

void SwScene::refreshDependencies() {
    // Clear Images
    {
        SwDependency& d = mPasses[SwPass::Type::ClearImages].getDeps();
        d.clear();
        d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
        d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentWrite);
        d.mWriteImages.emplace_back(&mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
        d.mWriteImages.emplace_back(&mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
        d.mWriteImages.emplace_back(&mPick.getResources().mReadbackImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    }

    // Copy to Swapchain
    {
        SwDependency& d = mPasses[SwPass::Type::CopyToSwapchain].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::TransferSrc);
        d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage(), SwDependency::ImageDepType::TransferDst);
    }
}

void SwScene::refresh() { refreshDependencies(); }

SwScene::SwScene()
    : mInput(*this), mCull(*this), mPick(*this), mIBL(*this), mWBOIT(*this), mGeometry(*this), mPostProcess(*this), mLighting(*this), mGui(*this) {}

void SwScene::initialize() {
    mInput.initialize();
    mCamera.initialize();
    mGui.initialize();

    initializeResources();
    initializeMiscPasses();

    mCull.initialize();
    mPick.initialize();
    mIBL.initialize();
    mWBOIT.initialize();
    mLighting.initialize();
    mGeometry.initialize();
    mPostProcess.initialize();

    loadStandaloneLightAssets();
}

void SwScene::resize() {
    mCull.resize();
    mLighting.resize();
    mPick.resize();
    mWBOIT.resize();
    mPostProcess.resize();
}

void SwScene::insertPass(SwPass::Type type, std::function<void(vk::CommandBuffer)> callback, bool mustRun) { mPasses[type] = SwPass(type, callback, mustRun); }

std::uint32_t SwScene::registerInstance(std::uint32_t assetId, SwInstance::Data instanceData) {
    SwInstance instance(assetId, instanceData);
    std::uint32_t instanceId = instance.getId();
    mInstances.emplace(instanceId, std::move(instance));
    addInstanceLights(getAsset(assetId), instanceId);
    return instanceId;
}

void SwScene::addInstanceLights(SwAsset& asset, std::uint32_t instanceId) {
    for (auto& node : asset.getNodes()) {
        auto* lightNode = dynamic_cast<SwLightNode*>(node.get());
        if (lightNode == nullptr) {
            continue;
        }
        SwLight light(asset.getLights()[lightNode->getLightIndex()].getParams());
        light.setInstanceContext(asset.getId(), instanceId, lightNode->getRelativeNodeIndex());
        mLights.emplace(light.getId(), std::move(light));
    }
}

void SwScene::removeInstanceLights(std::uint32_t instanceId) {
    std::erase_if(mLights, [instanceId](const auto& entry) { return entry.second.getInstanceId() == instanceId; });
}

void SwScene::refreshLightIndices() {
    mLightIds.clear();
    mLightIds.reserve(mLights.size());

    // Resolve each light's scene instance index from its position inside its asset's instance list.
    std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, std::uint32_t>> assetInstanceLocalIndex;
    for (auto& asset : mAssets | std::views::values) {
        auto& localMap = assetInstanceLocalIndex[asset.getId()];
        const std::vector<std::uint32_t>& instanceIds = asset.getInstanceIds();
        for (std::uint32_t i = 0; i < instanceIds.size(); i++) {
            localMap[instanceIds[i]] = i;
        }
    }

    for (auto& [lightId, light] : mLights) {
        SwAsset& asset = mAssets.at(light.getAssetId());
        const std::uint32_t localIndex = assetInstanceLocalIndex.at(light.getAssetId()).at(light.getInstanceId());
        light.setNodeTransformIndex(asset.mFirstNodeTransformInScene + light.getRelativeNodeIndex());
        light.setInstanceIndex(asset.mFirstInstanceInScene + localIndex);
        mLightIds.emplace_back(lightId);
    }
}

void SwScene::loadAssets(const std::vector<std::filesystem::path>& paths) {
    for (const auto& path : paths) {
        auto shortPath = SwAsset::getNameFromFilePath(path);
        if (mAlreadyLoadedAssetsNames.contains(shortPath)) {
            continue;
        }

        auto fullPath = ASSETS_PATH / path;
        SwAsset loadedAsset(fullPath);
        auto [it, inserted] = mAssets.try_emplace(loadedAsset.getId(), std::move(loadedAsset));
        if (inserted) {
            it->second.createInstance(mCamera);
            mAlreadyLoadedAssetsNames.insert(shortPath);
        }
    }

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this](vk::CommandBuffer cmd) {
        for (auto& sceneVisibilityRisBuffer : mVisibilityRisBuffers) {
            cmd.fillBuffer(sceneVisibilityRisBuffer.getHandle(), 0, vk::WholeSize, 0);  // Clear to 0 to mark all render items as not visible again.
        }
    });
}

void SwScene::loadStandaloneLightAssets() {
    constexpr std::array<std::pair<SwLight::Type, const char*>, 3> lightAssetFiles{{
        {SwLight::Type::Directional, "directional.gltf"},
        {SwLight::Type::Point, "point.gltf"},
        {SwLight::Type::Spot, "spot.gltf"},
    }};

    for (const auto& [type, file] : lightAssetFiles) {
        std::filesystem::path lightPath = std::filesystem::path(LIGHTS_PATH) / file;
        SwAsset lightAsset(lightPath);
        lightAsset.setStandaloneLight(true);
        const std::uint32_t id = lightAsset.getId();
        mAssets.try_emplace(id, std::move(lightAsset));
        mStandaloneLightAssetIds[type] = id;
    }
}

void SwScene::spawnStandaloneLight(SwLight::Type type) {
    const auto it = mStandaloneLightAssetIds.find(type);
    if (it == mStandaloneLightAssetIds.end()) {
        return;
    }
    mAssets.at(it->second).createInstance(mCamera);
}

void SwScene::unloadAssetsAndInstances() {
    std::erase_if(mAssets, [&](std::pair<const std::uint32_t, SwAsset>& pair) {
        SwAsset& asset = pair.second;
        const bool assetDeleted = asset.isMarkedDelete();
        if (assetDeleted) {
            mAlreadyLoadedAssetsNames.erase(asset.getName());
            mFlags.mAssetUnloaded = true;
            asset.deferDestroyImages();
        }

        // A standalone-light asset is shared by every light of its type, so removing one light deletes just that
        // instance, not the whole asset. Instances are also erased when their owning asset is going away.
        std::erase_if(asset.getInstanceIds(), [&](std::uint32_t instanceId) {
            SwInstance& instance = mInstances.at(instanceId);
            const bool instanceDeleted = assetDeleted || instance.isMarkedDelete();
            if (!instanceDeleted) {
                return false;
            }
            if (getPickSystem().getSelectedInstanceId() == instanceId) {
                getPickSystem().clearSelection();
            }
            if (!asset.getLights().empty()) {
                removeInstanceLights(instanceId);
            }
            if (!assetDeleted) {
                mFlags.mInstanceUnloaded = true;
                asset.setReloadInstancesFlag(true);  // repack the asset's instance buffer without the removed instance
            }
            mInstances.erase(instanceId);
            return true;
        });

        return assetDeleted;
    });
}

void SwScene::markAllAssetsDelete() {
    for (auto& asset : mAssets | std::views::values) {
        asset.markDelete();
    }
}

void SwScene::registerLoadedAsset(std::uint32_t assetId) { mAssetsIdsToFill.insert(assetId); }

void SwScene::fillAssetImages() {
    std::vector<SwColorImage2D*> uploadedImages;
    for (std::uint32_t assetId : mAssetsIdsToFill) {
        auto it = mAssets.find(assetId);
        if (it == mAssets.end()) continue;
        SwAsset& asset = it->second;
        for (auto& imageInfo : asset.getImageDataPtrs()) {
            std::uint32_t imageId = imageInfo.first;
            void* imageDataPtr = imageInfo.second;
            SwColorImage2D& image = *asset.getImages()[imageId];
            image.fillImageData(imageDataPtr, false);
            uploadedImages.emplace_back(&image);
        }
    }

    if (uploadedImages.empty()) return;

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [images = std::move(uploadedImages)](vk::CommandBuffer cmd) {
        for (SwColorImage2D* image : images) {
            image->emitTransition(cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    });
}

void SwScene::freeAssetImages() {
    for (std::uint32_t assetId : mAssetsIdsToFree) {
        auto it = mAssets.find(assetId);
        if (it == mAssets.end()) continue;
        SwAsset& asset = it->second;
        for (auto& imageInfo : asset.getImageDataPtrs()) {
            void* imageDataPtr = imageInfo.second;
            stbi_image_free(imageDataPtr);
        }
        asset.clearImageDataPtrs();
    }
}

void SwScene::fillAssetBuffers() {
    for (std::uint32_t assetId : mAssetsIdsToFill) {
        auto it = mAssets.find(assetId);
        if (it == mAssets.end()) continue;
        it->second.fillBuffers();
    }
}

void SwScene::freeAssetBuffers() {
    for (std::uint32_t assetId : mAssetsIdsToFree) {
        auto it = mAssets.find(assetId);
        if (it == mAssets.end()) continue;
        it->second.clearVerticesAndIndicesVectors();
    }
}

void SwScene::recordPendingDraw(SwMaterial& material, const SwRenderCommand& rc, std::uint32_t instanceCount) {
    mPendingRcs.emplace_back(rc, material.getType(), material.getPipelineBundle().getID(), instanceCount);
}

void SwScene::regenerateRcsAndRis() {
    SwBatch::sFirstRiOffset = 0;
    mPendingRcs.clear();
    mRcs.clear();
    mRis.clear();
    for (auto type : {SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent}) {
        mBatches[type].clear();
    }
    mBatchIndicesKeys.clear();
    for (auto& asset : mAssets | std::views::values) {
        if (asset.getInstanceIds().empty()) continue;
        asset.generateRcsAndRis();
    }
}

void SwScene::reloadRcsAndRisBuffers() {
    // Group the render commands by material type then pipeline so each batch occupies one contiguous range of the scene-wide arrays.
    std::ranges::stable_sort(mPendingRcs, [](const PendingRenderCommand& a, const PendingRenderCommand& b) {
        if (a.mMaterialType != b.mMaterialType) return a.mMaterialType < b.mMaterialType;
        return a.mPipelineId < b.mPipelineId;
    });

    std::uint32_t batchIndex = 0;
    for (std::size_t batchStart = 0; batchStart < mPendingRcs.size();) {
        const SwMaterial::Type materialType = mPendingRcs[batchStart].mMaterialType;
        const std::uint32_t pipelineId = mPendingRcs[batchStart].mPipelineId;

        const std::size_t rcsIndex = mRcs.size();
        const std::size_t risIndex = mRis.size();

        std::size_t batchEnd = batchStart;
        for (; batchEnd < mPendingRcs.size() && mPendingRcs[batchEnd].mMaterialType == materialType && mPendingRcs[batchEnd].mPipelineId == pipelineId;
             batchEnd++) {
            PendingRenderCommand& pending = mPendingRcs[batchEnd];
            const std::uint32_t rcIndex = static_cast<std::uint32_t>(mRcs.size());
            pending.mRc.mFirstRi = SwBatch::sFirstRiOffset;
            pending.mRc.mRiCount = 0;  // Render item count starts at zero and is incremented inside the culling compute shader.
            pending.mRc.mBatchIndex = batchIndex;
            mRcs.emplace_back(pending.mRc);
            for (std::uint32_t i = 0; i < pending.mNumInstance; i++) {
                mRis.emplace_back(rcIndex, pending.mRc.mFirstInstance + i);
            }
            SwBatch::sFirstRiOffset += pending.mNumInstance;
        }

        mBatches[materialType].try_emplace(
            pipelineId, materialType, pipelineId, batchIndex, rcsIndex, mRcs.size() - rcsIndex, risIndex, mRis.size() - risIndex
        );
        mBatchIndicesKeys.try_emplace({materialType, pipelineId}, batchIndex);
        batchIndex++;
        batchStart = batchEnd;
    }

    if (mRcs.empty()) return;

    const std::uint64_t rcsBytes = mRcs.size() * sizeof(SwRenderCommand);
    const std::uint64_t risBytes = mRis.size() * sizeof(SwRenderItem);
    const std::uint64_t risIndicesBytes = mRis.size() * sizeof(std::uint32_t);

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, rcsBytes, risBytes, risIndicesBytes](vk::CommandBuffer cmd) {
        SwStagingRing* stagingRing = SwRenderer::sRendererContext.mStagingRing;

        mInitialRcsBuffer.ensureCapacity(cmd, rcsBytes);
        cmd.fillBuffer(mInitialRcsBuffer.getHandle(), 0, vk::WholeSize, 0);
        mInitialRcsBuffer.emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
        stagingRing->upload(cmd, mInitialRcsBuffer, mRcs.data(), rcsBytes);

        mRisBuffer.ensureCapacity(cmd, risBytes);
        cmd.fillBuffer(mRisBuffer.getHandle(), 0, vk::WholeSize, 0);
        mRisBuffer.emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
        stagingRing->upload(cmd, mRisBuffer, mRis.data(), risBytes);

        mRisIndicesBuffer.ensureCapacity(cmd, risIndicesBytes);
        for (SwAllocatedBuffer& sceneVisibilityRisBuffer : mVisibilityRisBuffers) {
            sceneVisibilityRisBuffer.ensureCapacity(cmd, risIndicesBytes);
        }

        mEarlyRcsBuffer.ensureCapacity(cmd, rcsBytes);  // At least as big as mInitialRcsBuffer
        mLateRcsBuffer.ensureCapacity(cmd, rcsBytes);   // At least as big as mInitialRcsBuffer
    });

    mLighting.regenerateShadowsRcs();
}

void SwScene::reloadBatchesBuffer() {
    if (mBatchIndicesKeys.size() == 0) return;
    std::vector<SwBatch::Data> batchData(mBatchIndicesKeys.size());

    for (auto& batchIndexKey : mBatchIndicesKeys) {
        SwMaterial::Type materialType = batchIndexKey.first.first;
        std::uint32_t pipelineId = batchIndexKey.first.second;
        std::uint32_t batchIndex = batchIndexKey.second;
        SwBatch& batch = mBatches[materialType][pipelineId];
        batchData[batchIndex] = batch.toData();
    }

    const std::uint64_t batchDataSize = batchData.size() * sizeof(SwBatch::Data);
    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, batchData = std::move(batchData), batchDataSize](vk::CommandBuffer cmd) {
        if (batchDataSize == 0) return;
        SwRenderer::sRendererContext.mStagingRing->upload(cmd, mBatchesBuffer, batchData.data(), batchDataSize);
    });
}

void SwScene::realignVertexIndexOffset() {
    std::uint32_t vertexCumulative = 0;
    std::uint32_t indexCumulative = 0;
    for (auto& asset : mAssets | std::views::values) {
        for (auto& mesh : asset.getMeshes()) {
            mesh.mVertexOffsetInScene = vertexCumulative;
            mesh.mFirstIndexInScene = indexCumulative;
            vertexCumulative += mesh.mNumVertices;
            indexCumulative += mesh.mNumIndices;
        }
    }
}

void SwScene::realignMaterialOffset() {
    std::uint32_t materialCumulative = 0;
    for (auto& asset : mAssets | std::views::values) {
        asset.mFirstMaterialInScene = materialCumulative;
        materialCumulative += asset.getMaterials().size();
    }
}

void SwScene::realignNodeTransformsOffset() {
    std::uint32_t nodeTransformCumulative = 0;
    for (auto& asset : mAssets | std::views::values) {
        asset.mFirstNodeTransformInScene = nodeTransformCumulative;
        nodeTransformCumulative += asset.getNodes().size();
    }
}

void SwScene::realignBoundsOffset() {
    std::uint32_t boundsCumulative = 0;
    for (auto& asset : mAssets | std::views::values) {
        asset.mFirstBoundInScene = boundsCumulative;
        boundsCumulative += asset.getMeshes().size();
    }
}

void SwScene::realignInstancesOffset() {
    std::uint32_t instanceCumulative = 0;
    for (auto& asset : mAssets | std::views::values) {
        asset.mFirstInstanceInScene = instanceCumulative;
        instanceCumulative += asset.getInstanceIds().size();
    }
}

void SwScene::realignOffsets() {
    realignVertexIndexOffset();
    realignMaterialOffset();
    realignNodeTransformsOffset();
    realignBoundsOffset();
    realignInstancesOffset();
}

void SwScene::reloadVertexBuffer() {
    vk::DeviceSize dstOffset = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy vertexCopy{};
        vertexCopy.dstOffset = dstOffset;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = asset.getNumVertices() * sizeof(SwVertex);

        dstOffset += vertexCopy.size;

        SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&asset, this, vertexCopy](vk::CommandBuffer cmd) {
            if (vertexCopy.size == 0) return;
            mVertexBuffer.copyFrom(cmd, asset.getVertexBuffer(), vertexCopy);
        });
    }
}

void SwScene::reloadIndexBuffer() {
    vk::DeviceSize dstOffset = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy indexCopy{};
        indexCopy.dstOffset = dstOffset;
        indexCopy.srcOffset = 0;
        indexCopy.size = asset.getNumIndices() * sizeof(std::uint32_t);

        dstOffset += indexCopy.size;

        SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&asset, this, indexCopy](vk::CommandBuffer cmd) {
            if (indexCopy.size == 0) return;
            mIndexBuffer.copyFrom(cmd, asset.getIndexBuffer(), indexCopy);
        });
    }
}

void SwScene::reloadMaterialConstantsBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy materialConstantCopy{};
        materialConstantCopy.dstOffset = dstOffset;
        materialConstantCopy.srcOffset = 0;
        materialConstantCopy.size = asset.getMaterials().size() * sizeof(SwMaterial::Constant);

        dstOffset += materialConstantCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&asset, this, materialConstantCopy, maxPos](vk::CommandBuffer cmd) {
            if (materialConstantCopy.size == 0) return;
            mMaterialConstantsBuffer.copyFrom(cmd, asset.getMaterialConstantsBuffer(), materialConstantCopy);
        });
    }
}

void SwScene::reloadNodeTransformsBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy nodeTransformsCopy{};
        nodeTransformsCopy.dstOffset = dstOffset;
        nodeTransformsCopy.srcOffset = 0;
        nodeTransformsCopy.size = asset.getNodes().size() * sizeof(glm::mat4);

        dstOffset += nodeTransformsCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&asset, this, nodeTransformsCopy, maxPos](vk::CommandBuffer cmd) {
            if (nodeTransformsCopy.size == 0) return;
            mNodeTransformsBuffer.copyFrom(cmd, asset.getNodeTransformsBuffer(), nodeTransformsCopy);
        });
    }
}

void SwScene::reloadBoundsBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy boundsCopy{};
        boundsCopy.dstOffset = dstOffset;
        boundsCopy.srcOffset = 0;
        boundsCopy.size = asset.getMeshes().size() * sizeof(SwBounds);

        dstOffset += boundsCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&asset, this, boundsCopy, maxPos](vk::CommandBuffer cmd) {
            if (boundsCopy.size == 0) return;
            mBoundsBuffer.copyFrom(cmd, asset.getBoundsBuffer(), boundsCopy);
        });
    }
}

void SwScene::reloadInstancesBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        if (asset.getInstanceIds().empty()) {
            continue;
        }

        vk::BufferCopy instancesCopy{};
        instancesCopy.dstOffset = dstOffset;
        instancesCopy.srcOffset = 0;
        instancesCopy.size = asset.getInstanceIds().size() * sizeof(SwInstance::Data);

        dstOffset += instancesCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&asset, this, instancesCopy, maxPos](vk::CommandBuffer cmd) {
            if (instancesCopy.size == 0) return;
            mInstancesBuffer.copyFrom(cmd, asset.getInstancesBuffer(), instancesCopy);
        });
    }
}

void SwScene::reloadLightsBuffer() {
    if (mLightIds.empty()) {
        return;
    }

    std::vector<SwLight::Data> lightData;
    lightData.reserve(mLightIds.size());
    for (std::uint32_t lightId : mLightIds) {
        lightData.emplace_back(mLights.at(lightId).toData());
    }

    vk::BufferCopy lightsCopy{};
    lightsCopy.dstOffset = 0;
    lightsCopy.srcOffset = 0;
    lightsCopy.size = lightData.size() * sizeof(SwLight::Data);

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, lightData = std::move(lightData), lightsCopy](vk::CommandBuffer cmd) {
        if (lightsCopy.size == 0) return;
        SwRenderer::sRendererContext.mStagingRing->upload(cmd, mLightsBuffer, lightData.data(), lightsCopy.size);
    });
}

void SwScene::reloadMaterialResourcesArray() {
    for (auto& asset : mAssets | std::views::values) {
        for (auto& material : asset.getMaterials()) {
            std::uint32_t materialTextureArrayIndex = (asset.mFirstMaterialInScene + material.mRelativeMaterialIndex) * SwMaterial::NUM_PBR_IMAGES;
            std::array<SwMaterialTexture*, SwMaterial::NUM_PBR_IMAGES> materialTextures = {
                &material.getResources().mBase,
                &material.getResources().mMetallicRoughness,
                &material.getResources().mNormal,
                &material.getResources().mOcclusion,
                &material.getResources().mEmissive
            };
            for (std::uint32_t i = 0; i < SwMaterial::NUM_PBR_IMAGES; i++) {
                mMaterialSamplersDescriptorSet.writeSampler(0, materialTextures[i]->getSampler().getHandle(), materialTextureArrayIndex + i);
                mMaterialTexturesDescriptorSet.writeImage(
                    0, materialTextures[i]->getImage().getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, materialTextureArrayIndex + i
                );
            }
            mMaterialSamplersDescriptorSet.pushWrites();
            mMaterialTexturesDescriptorSet.pushWrites();
        }
    }
}

void SwScene::reloadBuffers() {
    reloadVertexBuffer();
    reloadIndexBuffer();
    reloadMaterialConstantsBuffer();
    reloadInstancesBuffer();
    reloadNodeTransformsBuffer();
    reloadBoundsBuffer();
    reloadMaterialResourcesArray();
}

void SwScene::resetFlags() {
    mFlags.mAssetLoaded = false;
    mFlags.mAssetUnloaded = false;
    mFlags.mInstanceLoaded = false;
    mFlags.mInstanceUnloaded = false;
    mFlags.mReloadMainInstancesBuffer = false;
    mFlags.mLightEdited = false;
}

void SwScene::startNextFrame() {
    SwFrame& currentFrame = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame();
    while (SwRenderer::sRendererContext.mDevice->waitForFences(currentFrame.getRenderFence().getHandle(), true, UINT64_MAX) == vk::Result::eTimeout) {
    }
    SwRenderer::sRendererContext.mDevice->resetFences(currentFrame.getRenderFence().getHandle());
    SwBufferFactory::tick(SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    SwImageFactory::tick(SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    SwRenderer::sRendererContext.mStagingRing->tick(SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    SwRenderer::sRendererContext.mSwapchain->acquireNextImage(1e9);
}

void SwScene::perFrameUpdate() {
    // Free last frame's uploaded CPU data before anything spawns new assets. Both frees consume mAssetsIdsToFree.
    freeAssetImages();
    freeAssetBuffers();
    mAssetsIdsToFree.clear();

    mGui.refresh();

    mCamera.update(SwRenderer::sRendererContext.mStats->mFrameTime, static_cast<float>(SwRenderer::ONE_SECOND_IN_MS / SwRenderer::EXPECTED_FRAME_RATE));

    unloadAssetsAndInstances();
    for (auto& asset : mAssets | std::views::values) {
        if (asset.getReloadInstancesFlag()) {
            asset.reloadInstances();
            mFlags.mReloadMainInstancesBuffer = true;
        }
    }

    // Upload newly loaded assets' per-buffer data before the scene consolidation reads those buffers below.
    fillAssetBuffers();

    if (mFlags.mAssetLoaded || mFlags.mAssetUnloaded) {
        realignOffsets();
        reloadVertexBuffer();
        reloadIndexBuffer();
        reloadMaterialConstantsBuffer();
        reloadInstancesBuffer();
        reloadNodeTransformsBuffer();
        reloadBoundsBuffer();
        reloadMaterialResourcesArray();
        regenerateRcsAndRis();
        reloadRcsAndRisBuffers();
        reloadBatchesBuffer();
        refreshLightIndices();
        reloadLightsBuffer();
    } else if (mFlags.mInstanceLoaded || mFlags.mInstanceUnloaded) {
        realignInstancesOffset();
        reloadInstancesBuffer();
        regenerateRcsAndRis();
        reloadRcsAndRisBuffers();
        reloadBatchesBuffer();
        refreshLightIndices();
        reloadLightsBuffer();
    } else if (mFlags.mReloadMainInstancesBuffer || mFlags.mLightEdited) {
        if (mFlags.mReloadMainInstancesBuffer) reloadInstancesBuffer();
        if (mFlags.mLightEdited) reloadLightsBuffer();
    }

    fillAssetImages();

    // Assets uploaded this frame become next frame's free set once the frame's draw has consumed their CPU data.
    mAssetsIdsToFree = std::move(mAssetsIdsToFill);
    mAssetsIdsToFill.clear();
}

void SwScene::draw() {
    SwFrame& currentFrame = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame();

    SwCommandBuffer& transferCommandBuffer = currentFrame.getTransferCommandBuffer();
    transferCommandBuffer.reset();
    transferCommandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    SwRenderer::sRendererContext.mImmSubmit->flushInto(SwQueueType::Transfer, transferCommandBuffer.getHandle());
    transferCommandBuffer.end();

    vk::CommandBufferSubmitInfo transferCommandBufferInfo = transferCommandBuffer.generateSubmitInfo();
    vk::SemaphoreSubmitInfo transferSignalInfo = currentFrame.getTransferSemaphore().generateSubmitInfo(vk::PipelineStageFlagBits2::eTransfer);
    vk::SubmitInfo2 transferSubmitInfo = {};
    transferSubmitInfo.commandBufferInfoCount = 1;
    transferSubmitInfo.pCommandBufferInfos = &transferCommandBufferInfo;
    transferSubmitInfo.signalSemaphoreInfoCount = 1;
    transferSubmitInfo.pSignalSemaphoreInfos = &transferSignalInfo;
    SwRenderer::sRendererContext.mTransferQueue->submit2(transferSubmitInfo, nullptr);

    SwCommandBuffer& graphicsCommandBuffer = currentFrame.getGraphicsCommandBuffer();
    graphicsCommandBuffer.reset();
    graphicsCommandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    SwRenderer::sRendererContext.mImmSubmit->flushInto(SwQueueType::Graphics, graphicsCommandBuffer.getHandle());

    // Write push constants and deps after flushing as callbacks may cause some buffers to resize.
    toggleVisibilityRisBuffer();
    mCull.refresh();
    mLighting.refresh();
    mPick.refresh();
    mIBL.refresh();
    mWBOIT.refresh();
    mGeometry.refresh();
    mPostProcess.refresh();
    refresh();

    mRenderGraph.addPass(&mPasses[SwPass::Type::ClearImages]);
    if (mIBL.isActive() && mIBL.isFileSelected()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::IBLSkybox]);
    }
    if (!mCull.getFreeze()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyReset]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyTest]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyCompact]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryEarlyOpaque]);
    if (!mCull.getFreeze()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullPrepOcclusion]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateReset]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateTest]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateCompact]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::CullPublishCount]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryZPass]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingClustersBuild]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingClustersMarkActive]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingClustersCompactActive]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingLightsCull]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingLightsFrustum]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingClustersLightCalcOffset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingClustersLightPrefixSumOffset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingClustersLightSelect]);
    // mRenderGraph.addPass(&mPasses[SwPass::Type::LightingShadowsCull]);
    // mRenderGraph.addPass(&mPasses[SwPass::Type::LightingShadowsDraw]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryLateOpaque]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryMasked]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryTransparent]);
    if (mPick.isPicked()) {  // This block always after geometry draws since it uses the same depth image
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickDraw]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickReadback]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickSelect]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::WBOITComposite]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::Tonemap]);
    if (mPostProcess.isFXAAActive()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::FXAA]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::CopyToSwapchain]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::Gui]);
    mRenderGraph.addOutput(&SwRenderer::sRendererContext.mSwapchain->getDrawImage());
    mRenderGraph.addOutput(&SwRenderer::sRendererContext.mSwapchain->getDepthImage());
    mRenderGraph.addOutput(&SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage());

    mRenderGraph.compile();
    mRenderGraph.execute(graphicsCommandBuffer);

    graphicsCommandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo = graphicsCommandBuffer.generateSubmitInfo();
    std::array<vk::SemaphoreSubmitInfo, 2> waitInfos = {
        currentFrame.getAvailableSemaphore().generateSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput),
        currentFrame.getTransferSemaphore().generateSubmitInfo(vk::PipelineStageFlagBits2::eAllCommands),
    };
    vk::SemaphoreSubmitInfo signalInfo = SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().getRenderedSemaphore().generateSubmitInfo(
        vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );
    SwRenderer::sRendererContext.mSwapchain->submit(commandBufferSubmitInfo, waitInfos, signalInfo, currentFrame.getRenderFence().getHandle());
    SwRenderer::sRendererContext.mSwapchain->present();
}