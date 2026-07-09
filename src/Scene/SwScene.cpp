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
    SwDependency staticDeps;

    // Clear Images
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentWrite);
    staticDeps.mWriteImages.emplace_back(&mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&mPick.getResources().mReadbackImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    mPasses[SwPass::Type::ClearImages] = SwPass(SwPass::Type::ClearImages, staticDeps, [&](vk::CommandBuffer cmd) {
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
    staticDeps.clear();

    // Copy to Swapchain
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::TransferSrc);
    mPasses[SwPass::Type::CopyToSwapchain] = SwPass(SwPass::Type::CopyToSwapchain, staticDeps, [&](vk::CommandBuffer cmd) {
        SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().copyFrom(cmd, SwRenderer::sRendererContext.mSwapchain->getDrawImage());
    });
    staticDeps.clear();
}

void SwScene::initializeResources() {
    mSceneVertexBuffer =
        SwBufferFactory::createAllocatedBuffer("SceneVertexBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_VERTEX_BUFFER_SIZE, true);
    mSceneIndexBuffer = SwBufferFactory::createAllocatedBuffer("SceneIndexBuffer", vk::BufferUsageFlagBits::eIndexBuffer, 0, SCENE_INITIAL_INDEX_BUFFER_SIZE);
    mSceneMaterialConstantsBuffer = SwBufferFactory::createAllocatedBuffer(
        "SceneMaterialConstantsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_MATERIAL_CONSTANTS_BUFFER_SIZE, true
    );
    mSceneNodeTransformsBuffer = SwBufferFactory::createAllocatedBuffer(
        "SceneNodeTransformsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_NODE_TRANSFORMS_BUFFER_SIZE, true
    );
    mSceneInstancesBuffer =
        SwBufferFactory::createAllocatedBuffer("SceneInstancesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_INSTANCES_BUFFER_SIZE, true);
    mSceneBoundsBuffer =
        SwBufferFactory::createAllocatedBuffer("SceneBoundsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_BOUNDS_BUFFER_SIZE, true);
    mSceneLightsBuffer =
        SwBufferFactory::createAllocatedBuffer("SceneLightsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_LIGHTS_BUFFER_SIZE, true);
    mSceneDrawRisIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "SceneDrawRisIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_RENDER_ITEMS_INDICES_BUFFER_SIZE, true
    );
    for (std::uint32_t i = 0; i < mSceneVisibilityRisBuffers.size(); i++) {
        mSceneVisibilityRisBuffers[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("SceneVisibilityRisBuffer{}", i), vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_RENDER_ITEMS_INDICES_BUFFER_SIZE, true
        );
    }
    mSceneInitialRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        "SceneInitialRcsBuffer",
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        SCENE_INITIAL_RENDER_COMMANDS_BUFFER_SIZE,
        true
    );
    mSceneEarlyRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        "SceneEarlyRcsBuffer",
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        SCENE_INITIAL_RENDER_COMMANDS_BUFFER_SIZE,
        true
    );
    mSceneEarlyRcsCount = SwBufferFactory::createAllocatedBuffer(
        "SceneEarlyRcsCount",
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        SCENE_INITIAL_BATCHES_COUNT_BUFFER_SIZE,
        true
    );
    mSceneLateRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        "SceneLateRcsBuffer",
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        SCENE_INITIAL_RENDER_COMMANDS_BUFFER_SIZE,
        true
    );
    mSceneLateRcsCount = SwBufferFactory::createAllocatedBuffer(
        "SceneLateRcsCount",
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        0,
        SCENE_INITIAL_BATCHES_COUNT_BUFFER_SIZE,
        true
    );
    mSceneRisBuffer =
        SwBufferFactory::createAllocatedBuffer("SceneRisBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SCENE_INITIAL_RENDER_ITEMS_BUFFER_SIZE, true);

    mSceneMaterialResourcesDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        "SceneMaterialResourcesDescriptorSet", SwMaterialResources::sMaterialResourcesDescriptorLayout, SCENE_INITIAL_NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES
    );

    // The normal slot of each material gets a flat (0,0,1) normal instead of white so unmapped surfaces keep their geometric normal.
    constexpr std::uint32_t normalSlot = static_cast<std::uint32_t>(SwMaterialTexture::Type::Normal);
    for (std::uint32_t i = 0; i < SCENE_INITIAL_NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES; i++) {
        SwMaterialTexture& seed =
            (i % SwMaterial::NUM_PBR_IMAGES == normalSlot) ? SwMaterialTexture::sDefaultFlatNormalTexture : SwMaterialTexture::sDefaultWhiteTexture;
        mSceneMaterialResourcesDescriptorSet.writeImage(
            0, seed.getImage().getMainImageViewHandle(), seed.getSampler().getHandle(), vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mSceneMaterialResourcesDescriptorSet.pushWrites();
}

void SwScene::refreshDynamicDependencies() {
    SwDependency dynamicDeps;

    // Copy to Swapchain
    dynamicDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage(), SwDependency::ImageDepType::TransferDst);
    mPasses[SwPass::Type::CopyToSwapchain].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();

    mGui.refreshDynamicDependencies();
}

void SwScene::refresh() { refreshDynamicDependencies(); }

void SwScene::finalPresentTransition(SwCommandBuffer& commandBuffer) {
    SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().emitTransition(commandBuffer.getHandle(), SwDependency::ImageDepType::PresentSrc);
}

SwScene::SwScene() : mCull(*this), mPick(*this), mIBL(*this), mWBOIT(*this), mGeometry(*this), mPostProcess(*this), mLighting(*this), mGui(*this) {}

void SwScene::initialize() {
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
    mPick.resize();
    mWBOIT.resize();
    mPostProcess.resize();
}

void SwScene::insertPass(SwPass::Type type, SwDependency deps, std::function<void(vk::CommandBuffer)> callback, bool mustRun) {
    mPasses[type] = SwPass(type, std::move(deps), callback, mustRun);
}

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

    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        for (auto& sceneVisibilityRisBuffer : mSceneVisibilityRisBuffers) {
            cmd.fillBuffer(sceneVisibilityRisBuffer.getHandle(), 0, vk::WholeSize, 0);  // Clear to 0 to mark all render items as not visible again.
        }
    });
}

void SwScene::loadStandaloneLightAssets() {
    constexpr std::array<std::pair<SwLight::Type, const char*>, 3> lightAssetFiles{{
        {SwLight::Type::Point, "point.gltf"},
        {SwLight::Type::Spot, "spot.gltf"},
        {SwLight::Type::Directional, "directional.gltf"},
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

    SwRenderer::sRendererContext.mImmSubmit->addCallback([images = std::move(uploadedImages)](vk::CommandBuffer cmd) {
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
        it->second.clearPendingBufferData();
    }
}

void SwScene::recordPendingDraw(SwMaterial& material, const SwRenderCommand& rc, std::uint32_t instanceCount) {
    mPendingRcs.emplace_back(rc, material.getPipelineBundle().getID(), instanceCount);
}

void SwScene::regenerateRcsAndRis() {
    SwBatch::sFirstRiOffset = 0;
    mPendingRcs.clear();
    mSceneRcs.clear();
    mSceneRis.clear();
    for (auto type : {SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent}) {
        mBatches[type].clear();
    }

    for (auto& asset : mAssets | std::views::values) {
        if (asset.getInstanceIds().empty()) continue;
        asset.generateRcsAndRis();
    }

    // Group the render commands by material type then pipeline so each batch occupies one contiguous range of the scene-wide arrays.
    std::ranges::stable_sort(mPendingRcs, [](const PendingRenderCommand& a, const PendingRenderCommand& b) {
        if (a.mRc.mMaterialType != b.mRc.mMaterialType) return a.mRc.mMaterialType < b.mRc.mMaterialType;
        return a.mPipelineId < b.mPipelineId;
    });

    // Iterate over each batch worth of RCs, to create RIs and place them inside the batch map as new batch.
    std::uint32_t batchIndex = 0;
    for (std::size_t batchStart = 0; batchStart < mPendingRcs.size();) {
        const SwMaterial::Type materialType = mPendingRcs[batchStart].mRc.mMaterialType;
        const std::uint32_t pipelineId = mPendingRcs[batchStart].mPipelineId;

        const std::size_t rcsIndex = mSceneRcs.size();
        const std::size_t risIndex = mSceneRis.size();

        std::size_t batchEnd = batchStart;
        for (; batchEnd < mPendingRcs.size() && mPendingRcs[batchEnd].mRc.mMaterialType == materialType && mPendingRcs[batchEnd].mPipelineId == pipelineId;
             batchEnd++) {
            PendingRenderCommand& pending = mPendingRcs[batchEnd];
            const std::uint32_t rcIndex = static_cast<std::uint32_t>(mSceneRcs.size());
            pending.mRc.mFirstRi = SwBatch::sFirstRiOffset;
            pending.mRc.mRiCount = 0;  // Render item count set to 0, incremented inside culling compute shader
            pending.mRc.mBatchIndex = batchIndex;
            mSceneRcs.emplace_back(pending.mRc);
            for (std::uint32_t i = 0; i < pending.mInstanceCount; i++) {
                mSceneRis.emplace_back(rcIndex, pending.mRc.mFirstInstance + i);
            }
            SwBatch::sFirstRiOffset += pending.mInstanceCount;
        }

        mBatches[materialType].try_emplace(
            pipelineId, pipelineId, batchIndex, rcsIndex, mSceneRcs.size() - rcsIndex, risIndex, mSceneRis.size() - risIndex
        );
        batchIndex++;
        batchStart = batchEnd;
    }

    if (!mSceneRcs.empty()) {
        const std::uint64_t rcsBytes = mSceneRcs.size() * sizeof(SwRenderCommand);
        const std::uint64_t risBytes = mSceneRis.size() * sizeof(SwRenderItem);

        SwRenderer::sRendererContext.mImmSubmit->addCallback([this, rcsBytes, risBytes](vk::CommandBuffer cmd) {
            SwStagingRing* stagingRing = SwRenderer::sRendererContext.mStagingRing;

            mSceneInitialRcsBuffer.ensureCapacity(cmd, rcsBytes);
            cmd.fillBuffer(mSceneInitialRcsBuffer.getHandle(), 0, vk::WholeSize, 0);
            mSceneInitialRcsBuffer.emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
            stagingRing->upload(cmd, mSceneInitialRcsBuffer, mSceneRcs.data(), rcsBytes);
            mSceneInitialRcsBuffer.emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);

            mSceneRisBuffer.ensureCapacity(cmd, risBytes);
            cmd.fillBuffer(mSceneRisBuffer.getHandle(), 0, vk::WholeSize, 0);
            mSceneRisBuffer.emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
            stagingRing->upload(cmd, mSceneRisBuffer, mSceneRis.data(), risBytes);
            mSceneRisBuffer.emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);

            mSceneEarlyRcsBuffer.ensureCapacity(cmd, rcsBytes);  // At least as big as mSceneInitialRcsBuffer
            mSceneLateRcsBuffer.ensureCapacity(cmd, rcsBytes);   // At least as big as mSceneInitialRcsBuffer
        });
    }

    mLighting.regenerateShadowRcs();
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

void SwScene::reloadSceneVertexBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        for (auto& mesh : asset.getMeshes()) {
            vk::BufferCopy meshVertexCopy{};
            meshVertexCopy.dstOffset = dstOffset;
            meshVertexCopy.srcOffset = 0;
            meshVertexCopy.size = mesh.mNumVertices * sizeof(SwVertex);

            dstOffset += meshVertexCopy.size;
            maxPos = dstOffset;

            SwRenderer::sRendererContext.mImmSubmit->addCallback([&mesh, this, meshVertexCopy, maxPos](vk::CommandBuffer cmd) {
                if (meshVertexCopy.size == 0) return;
                mSceneVertexBuffer.copyFrom(cmd, mesh.getVertexBuffer(), meshVertexCopy);
            });
        }
    }
}

void SwScene::reloadSceneIndexBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        for (auto& mesh : asset.getMeshes()) {
            vk::BufferCopy meshIndexCopy{};
            meshIndexCopy.dstOffset = dstOffset;
            meshIndexCopy.srcOffset = 0;
            meshIndexCopy.size = mesh.mNumIndices * sizeof(std::uint32_t);

            dstOffset += meshIndexCopy.size;
            maxPos = dstOffset;

            SwRenderer::sRendererContext.mImmSubmit->addCallback([&mesh, this, meshIndexCopy, maxPos](vk::CommandBuffer cmd) {
                if (meshIndexCopy.size == 0) return;
                mSceneIndexBuffer.copyFrom(cmd, mesh.getIndexBuffer(), meshIndexCopy);
            });
        }
    }
}

void SwScene::reloadSceneMaterialConstantsBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy materialConstantCopy{};
        materialConstantCopy.dstOffset = dstOffset;
        materialConstantCopy.srcOffset = 0;
        materialConstantCopy.size = asset.getMaterials().size() * sizeof(SwMaterialConstants);

        dstOffset += materialConstantCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, materialConstantCopy, maxPos](vk::CommandBuffer cmd) {
            if (materialConstantCopy.size == 0) return;
            mSceneMaterialConstantsBuffer.copyFrom(cmd, asset.getMaterialConstantsBuffer(), materialConstantCopy);
        });
    }
}

void SwScene::reloadSceneNodeTransformsBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy nodeTransformsCopy{};
        nodeTransformsCopy.dstOffset = dstOffset;
        nodeTransformsCopy.srcOffset = 0;
        nodeTransformsCopy.size = asset.getNodes().size() * sizeof(glm::mat4);

        dstOffset += nodeTransformsCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, nodeTransformsCopy, maxPos](vk::CommandBuffer cmd) {
            if (nodeTransformsCopy.size == 0) return;
            mSceneNodeTransformsBuffer.copyFrom(cmd, asset.getNodeTransformsBuffer(), nodeTransformsCopy);
        });
    }
}

void SwScene::reloadSceneBoundsBuffer() {
    vk::DeviceSize dstOffset = 0;
    vk::DeviceSize maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy boundsCopy{};
        boundsCopy.dstOffset = dstOffset;
        boundsCopy.srcOffset = 0;
        boundsCopy.size = asset.getMeshes().size() * sizeof(SwBounds);

        dstOffset += boundsCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, boundsCopy, maxPos](vk::CommandBuffer cmd) {
            if (boundsCopy.size == 0) return;
            mSceneBoundsBuffer.copyFrom(cmd, asset.getBoundsBuffer(), boundsCopy);
        });
    }
}

void SwScene::reloadSceneInstancesBuffer() {
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

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, instancesCopy, maxPos](vk::CommandBuffer cmd) {
            if (instancesCopy.size == 0) return;
            mSceneInstancesBuffer.copyFrom(cmd, asset.getInstancesBuffer(), instancesCopy);
        });
    }
}

void SwScene::reloadSceneLightsBuffer() {
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

    SwRenderer::sRendererContext.mImmSubmit->addCallback([this, lightData = std::move(lightData), lightsCopy](vk::CommandBuffer cmd) {
        if (lightsCopy.size == 0) return;
        SwRenderer::sRendererContext.mStagingRing->upload(cmd, mSceneLightsBuffer, lightData.data(), lightsCopy.size);
    });
}

void SwScene::reloadSceneMaterialResourcesArray() {
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
                mSceneMaterialResourcesDescriptorSet.writeImage(
                    0,
                    materialTextures[i]->getImage().getMainImageViewHandle(),
                    materialTextures[i]->getSampler().getHandle(),
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    materialTextureArrayIndex + i
                );
            }
            mSceneMaterialResourcesDescriptorSet.pushWrites();
        }
    }
}

void SwScene::reloadSceneBuffers() {
    reloadSceneVertexBuffer();
    reloadSceneIndexBuffer();
    reloadSceneMaterialConstantsBuffer();
    reloadSceneInstancesBuffer();
    reloadSceneNodeTransformsBuffer();
    reloadSceneBoundsBuffer();
    reloadSceneMaterialResourcesArray();
}

void SwScene::resetFlags() {
    mFlags.mAssetLoaded = false;
    mFlags.mAssetUnloaded = false;
    mFlags.mInstanceLoaded = false;
    mFlags.mInstanceUnloaded = false;
    mFlags.mReloadMainInstancesBuffer = false;
    mFlags.mLightEdited = false;
}

void SwScene::perFrameUpdate() {
    const auto start = std::chrono::system_clock::now();

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
        reloadSceneBuffers();
        regenerateRcsAndRis();
        refreshLightIndices();
        reloadSceneLightsBuffer();
    } else if (mFlags.mInstanceLoaded || mFlags.mInstanceUnloaded) {
        realignInstancesOffset();
        reloadSceneInstancesBuffer();
        regenerateRcsAndRis();
        refreshLightIndices();
        reloadSceneLightsBuffer();
    } else if (mFlags.mReloadMainInstancesBuffer || mFlags.mLightEdited) {
        if (mFlags.mReloadMainInstancesBuffer) {
            reloadSceneInstancesBuffer();
        }
        if (mFlags.mLightEdited) {
            reloadSceneLightsBuffer();  // light params changed; re-upload without touching indices
        }
    }
    resetFlags();

    fillAssetImages();

    // Assets uploaded this frame become next frame's free set once the frame's draw has consumed their CPU data.
    mAssetsIdsToFree = std::move(mAssetsIdsToFill);
    mAssetsIdsToFill.clear();

    mCull.refresh();
    mLighting.refresh();
    mPick.refresh();
    mIBL.refresh();
    mWBOIT.refresh();
    mGeometry.refresh();
    mPostProcess.refresh();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    SwRenderer::sRendererContext.mStats->mSceneUpdateTime = static_cast<float>(elapsed.count()) / SwRenderer::ONE_SECOND_IN_MS;
}

void SwScene::draw() {
    auto start = std::chrono::system_clock::now();

    SwFrame& currentFrame = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame();

    auto _ = SwRenderer::sRendererContext.mDevice->waitForFences(currentFrame.getRenderFence().getHandle(), true, 1e9);
    SwRenderer::sRendererContext.mDevice->resetFences(currentFrame.getRenderFence().getHandle());
    SwBufferFactory::tick(SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    SwImageFactory::tick(SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    SwRenderer::sRendererContext.mStagingRing->tick(SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    SwRenderer::sRendererContext.mSwapchain->acquireNextImage(1e9);

    refresh();

    SwCommandBuffer& commandBuffer = currentFrame.getCommandBuffer();
    commandBuffer.reset();
    commandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    SwRenderer::sRendererContext.mImmSubmit->flushInto(commandBuffer.getHandle());

    mRenderGraph.addPass(&mPasses[SwPass::Type::ClearImages]);
    if (mIBL.isActive() && mIBL.isFileSelected()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::IBLSkybox]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyWork]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyCompact]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryEarlyOpaque]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullPrepOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateWork]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateCompact]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullPublishCount]);
    /*mRenderGraph.addPass(&mPasses[SwPass::Type::LightingShadowReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingLightsCull]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingShadowCull]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::LightingShadowDraw]);*/
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryLateOpaque]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryMasked]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryTransparent]);
    if (mPick.isPicked()) {  // This block always after geometry draws since it uses the same depth image
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickDraw]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickReadback]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickWork]);
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
    mRenderGraph.execute(commandBuffer);
    finalPresentTransition(commandBuffer);

    commandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo = commandBuffer.generateSubmitInfo();
    vk::SemaphoreSubmitInfo waitInfo = currentFrame.getAvailableSemaphore().generateSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    vk::SemaphoreSubmitInfo signalInfo = SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().getRenderedSemaphore().generateSubmitInfo(
        vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );
    SwRenderer::sRendererContext.mSwapchain->submit(commandBufferSubmitInfo, waitInfo, signalInfo, currentFrame.getRenderFence().getHandle());
    SwRenderer::sRendererContext.mSwapchain->present();

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    SwRenderer::sRendererContext.mStats->mDrawTime = static_cast<float>(elapsed.count()) / SwRenderer::ONE_SECOND_IN_MS;
}