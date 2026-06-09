#include <Misc/SwHelper.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <quill/LogMacros.h>

#include <glm/glm.hpp>
#include <ranges>

void SwScene::initializeMiscPasses() {
    SwDependency staticDeps;

    // Clear Images
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentWrite);
    staticDeps.mWriteImages.emplace_back(&mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&mPick.getResources().mReadbackImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&mPick.getResources().mDepthImage, SwDependency::ImageDepType::DepthAttachmentWrite);
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

        depthAttachment = mPick.getResources().mDepthImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
        renderInfo = SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), nullptr, depthAttachment);

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
    mSceneVertexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_VERTEX_BUFFER_SIZE, true
    );

    mSceneIndexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eIndexBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_INDEX_BUFFER_SIZE
    );

    mSceneMaterialConstantsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_NUM_MATERIALS * sizeof(SwMaterialConstants), true
    );

    mSceneNodeTransformsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_NUM_NODES * sizeof(glm::mat4), true
    );

    mSceneInstancesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_NUM_INSTANCES * sizeof(SwInstance::Data), true
    );

    mSceneBoundsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_NUM_BOUNDS * sizeof(SwBounds), true
    );

    mSceneDrawRisIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t), true
    );

    for (std::uint32_t i = 0; i < mSceneVisibilityRisBuffers.size(); i++) {
        mSceneVisibilityRisBuffers[i] = SwBufferFactory::createAllocatedBuffer(
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t),
            true
        );
    }

    mSceneMaterialResourcesDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        SwMaterialResources::sMaterialResourcesDescriptorLayout, SCENE_INITIAL_NUM_MATERIALS * SwMaterial::NUM_PBR_IMAGES
    );
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
    SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().emitTransition(
        commandBuffer.getRawCommandBuffer(), SwDependency::ImageDepType::PresentSrc
    );
}

SwScene::SwScene() : mCull(*this), mPick(*this), mSkybox(*this), mWBOIT(*this), mGeometry(*this), mGui(*this) {}

void SwScene::initialize() {
    mCamera.initialize();
    mGui.initialize();

    initializeResources();
    initializeMiscPasses();

    mCull.initialize();
    mPick.initialize();
    mSkybox.initialize();
    mWBOIT.initialize();
    mGeometry.initialize();
}

void SwScene::resize() {
    mCull.resize();
    mPick.resize();
    mWBOIT.resize();
}

void SwScene::insertPass(SwPass::Type type, SwDependency deps, std::function<void(vk::CommandBuffer)> callback, bool mustRun) {
    mPasses[type] = SwPass(type, std::move(deps), callback, mustRun);
}

void SwScene::loadAssets(const std::vector<std::filesystem::path>& paths) {
    for (const auto& path : paths) {
        auto shortPath = SwAsset::getNameFromFilePath(path);
        if (mAlreadyLoadedAssets.contains(shortPath)) {
            continue;
        }

        auto fullPath = ASSETS_PATH / path;
        SwAsset loadedAsset(fullPath);
        auto [it, inserted] = mAssets.try_emplace(loadedAsset.getId(), std::move(loadedAsset));
        if (inserted) {
            it->second.createInstance(mCamera);
            mAlreadyLoadedAssets.insert(shortPath);
            mFlags.mAssetLoaded = true;
        }
    }

    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        for (auto& sceneVisibilityRisBuffer : mSceneVisibilityRisBuffers) {
            cmd.fillBuffer(sceneVisibilityRisBuffer.getRawBuffer(), 0, vk::WholeSize, 0);  // Clear to 0 to mark all render items as not visible again.
        }
    });
}

void SwScene::unloadAssets() {
    std::erase_if(mAssets, [&](std::pair<const std::uint32_t, SwAsset>& pair) {
        if (pair.second.isMarkedDelete()) {
            mAlreadyLoadedAssets.erase(pair.second.getName());
            mFlags.mAssetUnloaded = true;
        }
        return pair.second.isMarkedDelete();
    });
}

void SwScene::unloadInstances() {
    for (auto& asset : mAssets | std::views::values) {
        std::erase_if(asset.getInstances(), [&](const SwInstance& instance) { return instance.isMarkedDelete(); });
    }
}

void SwScene::markAllAssetsDelete() {
    for (auto& asset : mAssets | std::views::values) {
        asset.markDelete();
    }
}

void SwScene::regenerateRcsAndRis() {
    SwBatch::sFirstRiOffset = 0;

    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            batch.getRcs().clear();
            batch.getRis().clear();
        }
    }
    for (auto& asset : mAssets | std::views::values) {
        if (asset.getInstances().size() == 0) continue;
        asset.generateRcsAndRis();
    }

    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRcs().empty()) {
                continue;
            }

            vk::BufferCopy RcsCopy{};
            RcsCopy.dstOffset = 0;
            RcsCopy.srcOffset = 0;
            RcsCopy.size = batch.getRcs().size() * sizeof(SwRenderCommand);
            vk::BufferCopy RisCopy{};
            RisCopy.dstOffset = 0;
            RisCopy.srcOffset = 0;
            RisCopy.size = batch.getRis().size() * sizeof(SwRenderItem);

            SwRenderer::sRendererContext.mImmSubmit->addCallback([&batch, RcsCopy, RisCopy](vk::CommandBuffer cmd) {
                batch.getRcsStaging().copyFrom(cmd, batch.getRcs().data(), RcsCopy.size);
                cmd.fillBuffer(batch.getInitialRcsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                batch.getInitialRcsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
                batch.getInitialRcsBuffer().copyFrom(cmd, batch.getRcsStaging(), RcsCopy);
                batch.getInitialRcsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);

                batch.getRisStaging().copyFrom(cmd, batch.getRis().data(), RisCopy.size);
                cmd.fillBuffer(batch.getRisBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                batch.getRisBuffer().emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
                batch.getRisBuffer().copyFrom(cmd, batch.getRisStaging(), RisCopy);
                batch.getRisBuffer().emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);

                batch.getEarlyRcsBuffer().ensureCapacity(cmd, RcsCopy.size);  // At least as big as mInitialRcsBuffer
                batch.getLateRcsBuffer().ensureCapacity(cmd, RcsCopy.size);   // At least as big as mInitialRcsBuffer
            });
        }
    }
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
        instanceCumulative += asset.getInstances().size();
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
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        for (auto& mesh : asset.getMeshes()) {
            vk::BufferCopy meshVertexCopy{};
            meshVertexCopy.dstOffset = dstOffset;
            meshVertexCopy.srcOffset = 0;
            meshVertexCopy.size = mesh.mNumVertices * sizeof(SwVertex);

            dstOffset += meshVertexCopy.size;
            maxPos = dstOffset;

            SwRenderer::sRendererContext.mImmSubmit->addCallback([&mesh, this, meshVertexCopy, maxPos](vk::CommandBuffer cmd) {
                mSceneVertexBuffer.copyFrom(cmd, mesh.getVertexBuffer(), meshVertexCopy);
            });
        }
    }
}

void SwScene::reloadSceneIndexBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        for (auto& mesh : asset.getMeshes()) {
            vk::BufferCopy meshIndexCopy{};
            meshIndexCopy.dstOffset = dstOffset;
            meshIndexCopy.srcOffset = 0;
            meshIndexCopy.size = mesh.mNumIndices * sizeof(std::uint32_t);

            dstOffset += meshIndexCopy.size;
            maxPos = dstOffset;

            SwRenderer::sRendererContext.mImmSubmit->addCallback([&mesh, this, meshIndexCopy, maxPos](vk::CommandBuffer cmd) {
                mSceneIndexBuffer.copyFrom(cmd, mesh.getIndexBuffer(), meshIndexCopy);
            });
        }
    }
}

void SwScene::reloadSceneMaterialConstantsBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy materialConstantCopy{};
        materialConstantCopy.dstOffset = dstOffset;
        materialConstantCopy.srcOffset = 0;
        materialConstantCopy.size = asset.getMaterials().size() * sizeof(SwMaterialConstants);

        dstOffset += materialConstantCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, materialConstantCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneMaterialConstantsBuffer.copyFrom(cmd, asset.getMaterialConstantsBuffer(), materialConstantCopy);
        });
    }
}

void SwScene::reloadSceneNodeTransformsBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy nodeTransformsCopy{};
        nodeTransformsCopy.dstOffset = dstOffset;
        nodeTransformsCopy.srcOffset = 0;
        nodeTransformsCopy.size = asset.getNodes().size() * sizeof(glm::mat4);

        dstOffset += nodeTransformsCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, nodeTransformsCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneNodeTransformsBuffer.copyFrom(cmd, asset.getNodeTransformsBuffer(), nodeTransformsCopy);
        });
    }
}

void SwScene::reloadSceneBoundsBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy boundsCopy{};
        boundsCopy.dstOffset = dstOffset;
        boundsCopy.srcOffset = 0;
        boundsCopy.size = asset.getMeshes().size() * sizeof(SwBounds);

        dstOffset += boundsCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, boundsCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneBoundsBuffer.copyFrom(cmd, asset.getBoundsBuffer(), boundsCopy);
        });
    }
}

void SwScene::reloadSceneInstancesBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        if (asset.getInstances().empty()) {
            continue;
        }

        vk::BufferCopy instancesCopy{};
        instancesCopy.dstOffset = dstOffset;
        instancesCopy.srcOffset = 0;
        instancesCopy.size = asset.getInstances().size() * sizeof(SwInstance::Data);

        dstOffset += instancesCopy.size;
        maxPos = dstOffset;

        SwRenderer::sRendererContext.mImmSubmit->addCallback([&asset, this, instancesCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneInstancesBuffer.copyFrom(cmd, asset.getInstancesBuffer(), instancesCopy);
        });
    }
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
                    materialTextures[i]->getImage().getRawMainImageView(),
                    materialTextures[i]->getSampler().getRawSampler(),
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
}

void SwScene::perFrameUpdate() {
    mGui.refresh();

    const auto start = std::chrono::system_clock::now();

    mCamera.update(SwRenderer::sRendererContext.mStats->mFrameTime, static_cast<float>(SwRenderer::ONE_SECOND_IN_MS / SwRenderer::EXPECTED_FRAME_RATE));

    unloadAssets();
    unloadInstances();

    for (auto& asset : mAssets | std::views::values) {
        if (asset.getReloadInstancesFlag()) {
            asset.reloadInstances();
            mFlags.mReloadMainInstancesBuffer = true;
        }
    }

    if (mFlags.mAssetLoaded || mFlags.mAssetUnloaded) {
        realignOffsets();
        reloadSceneBuffers();
        regenerateRcsAndRis();
    } else if (mFlags.mInstanceLoaded || mFlags.mInstanceUnloaded) {
        realignInstancesOffset();
        reloadSceneInstancesBuffer();
        regenerateRcsAndRis();
    } else if (mFlags.mReloadMainInstancesBuffer) {
        reloadSceneInstancesBuffer();
    }

    resetFlags();

    mCull.refresh();
    mPick.refresh();
    mSkybox.refresh();
    mWBOIT.refresh();
    mGeometry.refresh();

    SwRenderer::sRendererContext.mImmSubmit->queuedSubmit();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    SwRenderer::sRendererContext.mStats->mSceneUpdateTime = static_cast<float>(elapsed.count()) / SwRenderer::ONE_SECOND_IN_MS;
}

void SwScene::draw() {
    auto start = std::chrono::system_clock::now();

    SwFrame& currentFrame = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame();

    auto _ = SwRenderer::sRendererContext.mDevice->waitForFences(currentFrame.getRenderFence().getRawFence(), true, 1e9);
    SwRenderer::sRendererContext.mDevice->resetFences(currentFrame.getRenderFence().getRawFence());
    SwBufferFactory::tick(SwRenderer::sRendererContext.mSwapchain->getFrameNumber());
    SwRenderer::sRendererContext.mSwapchain->acquireNextImage(1e9);

    refresh();

    SwCommandBuffer& commandBuffer = currentFrame.getCommandBuffer();
    commandBuffer.reset();
    commandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    mRenderGraph.addPass(&mPasses[SwPass::Type::ClearImages]);
    if (mSkybox.isActive() && mSkybox.isFileSelected()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::SkyboxWork]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyWork]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullEarlyCompact]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryEarlyOpaque]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullPrepOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateWork]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullLateCompact]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullPublishCount]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryLateOpaque]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryMasked]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryTransparent]);
    if (mPick.isPicked()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickDraw]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickReadback]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickWork]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::WBOITComposite]);
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
    SwRenderer::sRendererContext.mSwapchain->submit(commandBufferSubmitInfo, waitInfo, signalInfo, currentFrame.getRenderFence().getRawFence());
    SwRenderer::sRendererContext.mSwapchain->present();

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    SwRenderer::sRendererContext.mStats->mDrawTime = static_cast<float>(elapsed.count()) / SwRenderer::ONE_SECOND_IN_MS;
}