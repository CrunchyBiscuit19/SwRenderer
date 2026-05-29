#include <Misc/SwHelper.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <ranges>

SwRendererContext SwScene::sRendererContext{};

void SwScene::initializeMiscPasses() {
    SwDependency deps;

    // Clear Images
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentWrite);
    deps.mWriteImages.emplace_back(&mWBOIT.getResources().mAccumImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mWBOIT.getResources().mRvlImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mPick.getResources().mReadbackImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mPick.getResources().mDepthImage, SwDependency::ImageDepType::DepthAttachmentWrite);
    mPasses[SwPass::Type::ClearImages] = SwPass(SwPass::Type::ClearImages, deps, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 4> colorAttachments = {
            sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mWBOIT.getResources().mAccumImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mWBOIT.getResources().mRvlImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mPick.getResources().mReadbackImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
        };
        vk::RenderingAttachmentInfo depthAttachment = sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachments, depthAttachment);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();

        depthAttachment = mPick.getResources().mDepthImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
        renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), nullptr, depthAttachment);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();
    });
    deps.clear();

    // Copy to Swapchain
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::TransferSrc);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getCurrentSwapchainImage(), SwDependency::ImageDepType::ShaderSampledRead);
    mPasses[SwPass::Type::CopyToSwapchain] = SwPass(SwPass::Type::CopyToSwapchain, deps, [&](vk::CommandBuffer cmd) {
        sRendererContext.mSwapchain->getDrawImage().copyFrom(cmd, sRendererContext.mSwapchain->getCurrentSwapchainImage());
    });
    deps.clear();

    // Gui
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getCurrentSwapchainImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    mPasses[SwPass::Type::Gui] = SwPass(SwPass::Type::Gui, deps, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 2> colorAttachments = {
            sRendererContext.mSwapchain->getCurrentSwapchainImage().generateRenderingAttachment(vk::AttachmentLoadOp::eDontCare),
            sRendererContext.mSwapchain->getCurrentSwapchainImage().generateRenderingAttachment(0, vk::AttachmentLoadOp::eDontCare),
        };
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachments, nullptr);

        cmd.beginRendering(renderInfo);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        cmd.endRendering();
    });
}

void SwScene::initializeResources() {
    mSceneVertexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_VERTEX_BUFFER_SIZE
    );

    mSceneIndexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INDEX_BUFFER_SIZE
    );

    mSceneMaterialConstantsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_MATERIALS * sizeof(SwMaterialConstants)
    );

    mSceneNodeTransformsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_NODES * sizeof(glm::mat4)
    );

    mSceneInstancesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_INSTANCES * sizeof(SwInstance::Data)
    );

    mSceneBoundsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_BOUNDS * sizeof(SwBounds)
    );

    mSceneVisibleRenderInstancesInstanceIndexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_RENDER_INSTANCES * sizeof(std::uint32_t)
    );

    mSceneMaterialResourcesDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(
        SwMaterialResources::sMaterialResourcesDescriptorLayout, SwMaterialResources::MAX_TEXTURE_ARRAY_SLOTS
    );
}

void SwScene::finalPresentTransition(SwCommandBuffer& commandBuffer) {
    sRendererContext.mSwapchain->getCurrentSwapchainImage().emitTransition(
        commandBuffer.getRawCommandBuffer(), SwDependency::ImageDepType::PresentSrc);
}

SwScene::SwScene() : mCull(*this), mPick(*this), mSkybox(*this), mWBOIT(*this), mGeometry(*this) {}

void SwScene::init(SwRendererContext rendererContext) {
    sRendererContext = rendererContext;
    SwSystem::sRendererContext = rendererContext;
}

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

        auto fullPath = MODELS_PATH / path;
        SwAsset loadedAsset(fullPath);
        auto [_, inserted] = mAssets.try_emplace(loadedAsset.getId(), std::move(loadedAsset));
        if (inserted) {
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

void SwScene::regenerateRenderItemsInstances() {
    SwBatch::sFirstRenderInstanceOffset = 0;

    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            batch.getRenderItems().clear();
            batch.getRenderInstances().clear();
        }
    }
    for (auto& asset : mAssets | std::views::values) {
        asset.generateRenderItemsAndRenderInstances();
    }

    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) {
                continue;
            }

            std::memcpy(
                batch.getRenderItemsStagingBuffer().getMappedPointer(), batch.getRenderItems().data(), batch.getRenderItems().size() * sizeof(SwRenderItem)
            );
            vk::BufferCopy renderItemsCopy{};
            renderItemsCopy.dstOffset = 0;
            renderItemsCopy.srcOffset = 0;
            renderItemsCopy.size = batch.getRenderItems().size() * sizeof(SwRenderItem);
            std::memcpy(
                batch.getRenderInstancesStagingBuffer().getMappedPointer(),
                batch.getRenderInstances().data(),
                batch.getRenderInstances().size() * sizeof(SwRenderInstance)
            );
            vk::BufferCopy renderInstancesCopy{};
            renderInstancesCopy.dstOffset = 0;
            renderInstancesCopy.srcOffset = 0;
            renderInstancesCopy.size = batch.getRenderInstances().size() * sizeof(SwRenderInstance);

            sRendererContext.mImmSubmit->addCallback([&batch, renderItemsCopy, renderInstancesCopy](vk::CommandBuffer cmd) {
                cmd.fillBuffer(batch.getPreCullRenderItemsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                batch.getPreCullRenderItemsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
                batch.getPreCullRenderItemsBuffer().copyFrom(cmd, batch.getRenderItemsStagingBuffer(), renderItemsCopy, renderItemsCopy.size);
                batch.getPreCullRenderItemsBuffer().emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);

                cmd.fillBuffer(batch.getRenderInstancesBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                batch.getRenderInstancesBuffer().emitBarrier(cmd, SwDependency::BufferDepType::TransferWrite);
                batch.getRenderInstancesBuffer().copyFrom(cmd, batch.getRenderInstancesStagingBuffer(), renderInstancesCopy, renderInstancesCopy.size);
                batch.getRenderInstancesBuffer().emitBarrier(cmd, SwDependency::BufferDepType::ComputeStorageRead);
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

void SwScene::reloadMainVertexBuffer() {
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

            sRendererContext.mImmSubmit->addCallback([&mesh, this, meshVertexCopy, maxPos](vk::CommandBuffer cmd) {
                mSceneVertexBuffer.copyFrom(cmd, mesh.getVertexBuffer(), meshVertexCopy, maxPos);
            });
        }
    }
}

void SwScene::reloadMainIndexBuffer() {
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

            sRendererContext.mImmSubmit->addCallback([&mesh, this, meshIndexCopy, maxPos](vk::CommandBuffer cmd) {
                mSceneIndexBuffer.copyFrom(cmd, mesh.getIndexBuffer(), meshIndexCopy, maxPos);
            });
        }
    }
}

void SwScene::reloadMainMaterialConstantsBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy materialConstantCopy{};
        materialConstantCopy.dstOffset = dstOffset;
        materialConstantCopy.srcOffset = 0;
        materialConstantCopy.size = asset.getMaterials().size() * sizeof(SwMaterialConstants);

        dstOffset += materialConstantCopy.size;
        maxPos = dstOffset;

        sRendererContext.mImmSubmit->addCallback([&asset, this, materialConstantCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneMaterialConstantsBuffer.copyFrom(cmd, asset.getMaterialConstantsBuffer(), materialConstantCopy, maxPos);
        });
    }
}

void SwScene::reloadMainNodeTransformsBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy nodeTransformsCopy{};
        nodeTransformsCopy.dstOffset = dstOffset;
        nodeTransformsCopy.srcOffset = 0;
        nodeTransformsCopy.size = asset.getNodes().size() * sizeof(glm::mat4);

        dstOffset += nodeTransformsCopy.size;
        maxPos = dstOffset;

        sRendererContext.mImmSubmit->addCallback([&asset, this, nodeTransformsCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneNodeTransformsBuffer.copyFrom(cmd, asset.getNodeTransformsBuffer(), nodeTransformsCopy, maxPos);
        });
    }
}

void SwScene::reloadMainBoundsBuffer() {
    std::uint32_t dstOffset = 0;
    std::uint32_t maxPos = 0;

    for (auto& asset : mAssets | std::views::values) {
        vk::BufferCopy boundsCopy{};
        boundsCopy.dstOffset = dstOffset;
        boundsCopy.srcOffset = 0;
        boundsCopy.size = asset.getMeshes().size() * sizeof(SwBounds);

        dstOffset += boundsCopy.size;
        maxPos = dstOffset;

        sRendererContext.mImmSubmit->addCallback([&asset, this, boundsCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneBoundsBuffer.copyFrom(cmd, asset.getBoundsBuffer(), boundsCopy, maxPos);
        });
    }
}

void SwScene::reloadMainInstancesBuffer() {
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

        sRendererContext.mImmSubmit->addCallback([&asset, this, instancesCopy, maxPos](vk::CommandBuffer cmd) {
            mSceneInstancesBuffer.copyFrom(cmd, asset.getInstancesBuffer(), instancesCopy, maxPos);
        });
    }
}

void SwScene::reloadMainMaterialResourcesArray() {
    for (auto& asset : mAssets | std::views::values) {
        for (auto& material : asset.getMaterials()) {
            std::uint32_t materialTextureArrayIndex = (asset.mFirstMaterialInScene + material.mRelativeMaterialIndex) * SwMaterial::NUM_PBR_IMAGES;
            std::array<SwMaterialTexture*, SwMaterial::NUM_PBR_IMAGES> materialTextures = {
                &material.getResources().mBase,
                &material.getResources().mMetallicRoughness,
                &material.getResources().mEmissive,
                &material.getResources().mNormal,
                &material.getResources().mOcclusion
            };
            for (std::uint32_t i = 0; i < SwMaterial::NUM_PBR_IMAGES; i++) {
                if (materialTextures[i]->getImage().getRawImage() == VK_NULL_HANDLE) {
                    mSceneMaterialResourcesDescriptorSet.writeImage(
                        0,
                        materialTextures[i]->getImage().getRawMainImageView(),
                        materialTextures[i]->getSampler().getRawSampler(),
                        vk::ImageLayout::eShaderReadOnlyOptimal,
                        vk::DescriptorType::eCombinedImageSampler,
                        materialTextureArrayIndex + i
                    );
                }
            }
            mSceneMaterialResourcesDescriptorSet.pushWrites();
        }
    }
}

void SwScene::reloadMainBuffers() {
    reloadMainVertexBuffer();
    reloadMainIndexBuffer();
    reloadMainMaterialConstantsBuffer();
    reloadMainInstancesBuffer();
    reloadMainNodeTransformsBuffer();
    reloadMainBoundsBuffer();
    reloadMainMaterialResourcesArray();
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

    mCamera.update(sRendererContext.mStats->mFrameTime, static_cast<float>(SwRenderer::ONE_SECOND_IN_MS / SwRenderer::EXPECTED_FRAME_RATE));

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
        reloadMainBuffers();
        regenerateRenderItemsInstances();
    } else if (mFlags.mInstanceLoaded || mFlags.mInstanceUnloaded) {
        realignInstancesOffset();
        reloadMainInstancesBuffer();
        regenerateRenderItemsInstances();
    } else if (mFlags.mReloadMainInstancesBuffer) {
        reloadMainInstancesBuffer();
    }

    resetFlags();

    sRendererContext.mImmSubmit->queuedSubmit();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    sRendererContext.mStats->mSceneUpdateTime = static_cast<float>(elapsed.count()) / SwRenderer::ONE_SECOND_IN_MS;
}

void SwScene::draw() {
    auto start = std::chrono::system_clock::now();

    SwFrame& currentFrame = sRendererContext.mSwapchain->getCurrentFrame();

    auto _ = sRendererContext.mDevice->waitForFences(currentFrame.getRenderFence().getRawFence(), true, 1e9);
    sRendererContext.mDevice->resetFences(currentFrame.getRenderFence().getRawFence());
    sRendererContext.mSwapchain->acquireNextImage(1e9);

    SwCommandBuffer& commandBuffer = currentFrame.getCommandBuffer();
    commandBuffer.reset();
    commandBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    mRenderGraph.addPass(&mPasses[SwPass::Type::ClearImages]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullReset]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullDepthPyramid]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullWork]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CullCompact]);
    if (mPick.isPicked()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickDraw]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickReadback]);
        mRenderGraph.addPass(&mPasses[SwPass::Type::PickWork]);
        mRenderGraph.addOutput(&mPick.getResources().mReadbackImage);
        mRenderGraph.addOutput(&mPick.getResources().mDepthImage);
    }
    if (mSkybox.isActive() && mSkybox.isDirSelected()) {
        mRenderGraph.addPass(&mPasses[SwPass::Type::SkyboxWork]);
    }
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryOpaque]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::GeometryTransparent]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::WBOITComposite]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::CopyToSwapchain]);
    mRenderGraph.addPass(&mPasses[SwPass::Type::Gui]);
    mRenderGraph.addOutput(&sRendererContext.mSwapchain->getDrawImage());
    mRenderGraph.addOutput(&sRendererContext.mSwapchain->getDepthImage());

    mRenderGraph.compile();
    mRenderGraph.execute(commandBuffer);
    finalPresentTransition(commandBuffer);

    commandBuffer.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo = commandBuffer.generateSubmitInfo();
    vk::SemaphoreSubmitInfo waitInfo = currentFrame.getAvailableSemaphore().generateSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    vk::SemaphoreSubmitInfo signalInfo =
        sRendererContext.mSwapchain->getCurrentSwapchainImage().getRenderedSemaphore().generateSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    sRendererContext.mSwapchain->submit(commandBufferSubmitInfo, waitInfo, signalInfo, currentFrame.getRenderFence().getRawFence());
    sRendererContext.mSwapchain->present();

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    sRendererContext.mStats->mDrawTime = static_cast<float>(elapsed.count()) / SwRenderer::ONE_SECOND_IN_MS;
}