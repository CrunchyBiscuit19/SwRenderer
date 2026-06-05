#include <Misc/SwHelper.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <imgui_impl_vulkan.h>
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
            SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), colorAttachments, depthAttachment);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();

        depthAttachment = mPick.getResources().mDepthImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
        renderInfo = SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), nullptr, depthAttachment);

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

    // Gui
    staticDeps.mReadBuffers.emplace_back(&SwRenderer::sRendererContext.mStats->mRInstsCount, SwDependency::BufferDepType::HostRead);
    mPasses[SwPass::Type::Gui] = SwPass(SwPass::Type::Gui, staticDeps, [&](vk::CommandBuffer cmd) {
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            SwRenderer::sRendererContext.mSwapchain->getWindowExtent(),
            SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().generateRenderingAttachment(vk::AttachmentLoadOp::eDontCare),
            nullptr
        );
        cmd.beginRendering(renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        cmd.endRendering();
    });
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

    mSceneVisibleRInstsIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INITIAL_NUM_RENDER_INSTANCES * sizeof(std::uint32_t), true
    );

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

    // Gui
    dynamicDeps.mWriteImages.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite
    );
    mPasses[SwPass::Type::Gui].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwScene::refresh() { refreshDynamicDependencies(); }

void SwScene::finalPresentTransition(SwCommandBuffer& commandBuffer) {
    SwRenderer::sRendererContext.mSwapchain->getCurrentSwapchainImage().emitTransition(
        commandBuffer.getRawCommandBuffer(), SwDependency::ImageDepType::PresentSrc
    );
}

SwScene::SwScene() : mCull(*this), mPick(*this), mSkybox(*this), mWBOIT(*this), mGeometry(*this) {}

void SwScene::initialize() {
    mCamera.initialize();

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

void SwScene::regenerateRItemsAndRInsts() {
    SwBatch::sFirstRInstOffset = 0;

    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            batch.getRItems().clear();
            batch.getRInsts().clear();
        }
    }
    for (auto& asset : mAssets | std::views::values) {
        if (asset.getInstances().size() == 0) continue;
        asset.generateRItemsAndRInsts();
    }

    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRItems().empty()) {
                continue;
            }

            vk::BufferCopy RItemsCopy{};
            RItemsCopy.dstOffset = 0;
            RItemsCopy.srcOffset = 0;
            RItemsCopy.size = batch.getRItems().size() * sizeof(SwRenderItem);
            vk::BufferCopy RInstsCopy{};
            RInstsCopy.dstOffset = 0;
            RInstsCopy.srcOffset = 0;
            RInstsCopy.size = batch.getRInsts().size() * sizeof(SwRenderInstance);

            SwRenderer::sRendererContext.mImmSubmit->addCallback([&batch, RItemsCopy, RInstsCopy](vk::CommandBuffer cmd) {
                batch.getRItemsStaging().copyFrom(cmd, batch.getRItems().data(), RItemsCopy.size);
                cmd.fillBuffer(batch.getInitialRItemsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                batch.getInitialRItemsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
                batch.getInitialRItemsBuffer().copyFrom(cmd, batch.getRItemsStaging(), RItemsCopy);
                batch.getInitialRItemsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);

                batch.getRInstsStaging().copyFrom(cmd, batch.getRInsts().data(), RInstsCopy.size);
                cmd.fillBuffer(batch.getRInstsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                batch.getRInstsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
                batch.getRInstsBuffer().copyFrom(cmd, batch.getRInstsStaging(), RInstsCopy);
                batch.getRInstsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);

                batch.getFrustumRItemsBuffer().ensureCapacity(cmd, RItemsCopy.size);  // At least as big as mInitialRItemsBuffer
                batch.getOcclusionRItemsBuffer().ensureCapacity(cmd, RItemsCopy.size);  // At least as big as mInitialRItemsBuffer
                batch.getFrustumVisibleRInstsBuffer().ensureCapacity(cmd, RInstsCopy.size);  // At least as big as mRInstsBuffer
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
        regenerateRItemsAndRInsts();
    } else if (mFlags.mInstanceLoaded || mFlags.mInstanceUnloaded) {
        realignInstancesOffset();
        reloadSceneInstancesBuffer();
        regenerateRItemsAndRInsts();
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
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullResetFrustum]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullWorkFrustum]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullCompactFrustum]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryDepthPrePass]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullPrepOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullResetOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullWorkOcclusion]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullCompactOcclusion]);
    if (mPick.isPicked()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickDraw]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickReadback]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickWork]);
    }
    if (mSkybox.isActive() && mSkybox.isFileSelected()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::SkyboxWork]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryOpaque]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryTransparent]);
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