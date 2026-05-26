#include <Misc/SwHelper.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <imgui_impl_vulkan.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <ranges>

std::filesystem::path SwScene::CULL_RESET_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerReset.comp.spv";
std::filesystem::path SwScene::CULL_WORK_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerCull.comp.spv";
std::filesystem::path SwScene::CULL_COMPACT_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerCompact.comp.spv";
std::filesystem::path SwScene::CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerDepthPyramid.comp.spv";
std::filesystem::path SwScene::PICK_DRAW_VERTEX_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "PickerDraw.vert.spv";
std::filesystem::path SwScene::PICK_DRAW_FRAGMENT_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "PickerDraw.frag.spv";
std::filesystem::path SwScene::PICK_WORK_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "PickerPick.comp.spv";
std::filesystem::path SwScene::SKYBOX_VERTEX_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Skybox.vert.spv";
std::filesystem::path SwScene::SKYBOX_FRAGMENT_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Skybox.frag.spv";
std::filesystem::path SwScene::WBOIT_VERTEX_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Composite.vert.spv";
std::filesystem::path SwScene::WBOIT_FRAGMENT_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Composite.frag.spv";

SwRendererContext SwScene::sRendererContext{};

void SwScene::initializeMiscPasses() {
    SwDependency deps;

    // Clear Images
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentWrite);
    deps.mWriteImages.emplace_back(&mWBOITResources.mAccumImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mWBOITResources.mRvlImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mPickResources.mReadbackImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mPickResources.mDepthImage, SwDependency::ImageDepType::DepthAttachmentWrite);
    mPasses[SwPass::Type::ClearImages] = SwPass(SwPass::Type::ClearImages, deps, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 4> colorAttachments = {
            sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mWBOITResources.mAccumImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mWBOITResources.mRvlImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
            mPickResources.mReadbackImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear),
        };
        vk::RenderingAttachmentInfo depthAttachment = sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachments, depthAttachment);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();

        depthAttachment = mPickResources.mDepthImage.generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
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

void SwScene::initializeSceneResources() {
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

    sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_DELETE] && mPickResources.mClickedInstance != nullptr && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mPickResources.mClickedInstance->markDelete();
        }
    });
}

void SwScene::initializeCullResources() {
    // Reset pass
    mCullResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwCull::ResetPC::getRange());

    SwShader resetShader = SwShaderFactory::createShader(CULL_RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({resetShader.getRawModule(), mCullResources.mResetPipelineLayout.getRawLayout()});

    // Depth pyramid pass
    mCullResources.mDepthPyramidDescriptorLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1},
         {1, vk::DescriptorType::eSampledImage, CULL_MAX_DEPTH_PYRAMID_LEVELS},
         {2, vk::DescriptorType::eStorageImage, CULL_MAX_DEPTH_PYRAMID_LEVELS}},
        vk::ShaderStageFlagBits::eCompute
    );

    mCullResources.mDepthPyramidDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mCullResources.mDepthPyramidDescriptorLayout);

    mCullResources.mDepthPyramidPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mCullResources.mDepthPyramidDescriptorLayout.getRawLayout(), SwCull::DepthPyramidPC::getRange());

    SwShader depthPyramidShader = SwShaderFactory::createShader(CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mDepthPyramidPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({depthPyramidShader.getRawModule(), mCullResources.mDepthPyramidPipelineLayout.getRawLayout()});

    // Work pass
    mCullResources.mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute);

    mCullResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mCullResources.mWorkDescriptorLayout);

    mCullResources.mWorkPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mCullResources.mWorkDescriptorLayout.getRawLayout(), SwCull::WorkPC::getRange());

    SwShader workShader = SwShaderFactory::createShader(CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mWorkPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mCullResources.mWorkPipelineLayout.getRawLayout()});

    // Compact pass
    mCullResources.mCompactPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwCull::CompactPC::getRange());

    SwShader compactShader = SwShaderFactory::createShader(CULL_COMPACT_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mCompactPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({compactShader.getRawModule(), mCullResources.mCompactPipelineLayout.getRawLayout()});

    // Everything that needs to be re-built on resize
    onResizeInitializeCullResources();
}

void SwScene::onResizeInitializeCullResources() {
    // Depth pyramid pass
    mCullResources.mDepthPyramidExtent = vk::Extent3D{
        SwHelper::previousPow2(sRendererContext.mSwapchain->getWindowExtent().width),
        SwHelper::previousPow2(sRendererContext.mSwapchain->getWindowExtent().height),
        1,
    };
    mCullResources.mDepthPyramidLevels = SwHelper::calculateMipMapLevels(mCullResources.mDepthPyramidExtent);
    mCullResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        nullptr, vk::Format::eR32Sfloat, mCullResources.mDepthPyramidExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );
    for (std::uint32_t i = 0; i < mCullResources.mDepthPyramidLevels; i++) {
        mCullResources.mDepthPyramidImage.addImageView(
            mCullResources.mDepthPyramidImage.getMainFormat(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, i, 1
        );
    }
    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mCullResources.mDepthPyramidImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );
    });

    mCullResources.mDepthPyramidDescriptorSet.writeImage(
        0,
        sRendererContext.mSwapchain->getDepthImage().getRawMainImageView(),
        nullptr,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::DescriptorType::eSampledImage
    );
    for (std::uint32_t i = 0; i < mCullResources.mDepthPyramidLevels; i++) {
        mCullResources.mDepthPyramidDescriptorSet.writeImage(
            1, mCullResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eSampledImage, i
        );
        mCullResources.mDepthPyramidDescriptorSet.writeImage(
            2, mCullResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage, i
        );
    }
    mCullResources.mDepthPyramidDescriptorSet.pushWrites();

    vk::Extent3D depthPyramidExtent = mCullResources.mDepthPyramidImage.getExtent();
    vk::Extent3D depthExtent = vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1};
    mCullResources.mDepthPyramidPushConstants.mDepthPyramidExtent = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
    mCullResources.mDepthPyramidPushConstants.mDepthFullExtent = glm::uvec2(depthExtent.width, depthExtent.height);
    mCullResources.mDepthPyramidPushConstants.mDepthFullRatio =
        glm::vec2(depthPyramidExtent.width / static_cast<float>(depthExtent.width), depthPyramidExtent.height / static_cast<float>(depthExtent.height));

    // Work pass
    mCullResources.mWorkDescriptorSet.writeImage(
        0, mCullResources.mDepthPyramidImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mCullResources.mWorkDescriptorSet.pushWrites();

    mCullResources.mWorkPushConstants.mRenderInstancesCountBuffer = sRendererContext.mStats->mRenderInstancesCountBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneBoundsBuffer = mSceneBoundsBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mFrustumBuffer = mCamera.getFrustumBuffer().getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mSceneNodeTransformsBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneInstancesBuffer = mSceneInstancesBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mSceneVisibleRenderInstancesInstanceIndexBuffer.getDeviceAddress().value();
    vk::Extent2D drawExtent = sRendererContext.mSwapchain->getWindowExtent();
    mCullResources.mWorkPushConstants.mDrawExtents = glm::vec2(drawExtent.width, drawExtent.height);
    mCullResources.mWorkPushConstants.mDepthPyramidExtents = glm::vec2(depthPyramidExtent.width, depthPyramidExtent.height);
}

void SwScene::initializeCullPasses() {
    SwDependency deps;

    // Cull Reset
    deps.mWriteBuffers.emplace_back(&sRendererContext.mStats->mRenderInstancesCountBuffer, SwDependency::BufferDepType::TransferWrite);
    deps.mWriteBuffers.emplace_back(&mSceneVisibleRenderInstancesInstanceIndexBuffer, SwDependency::BufferDepType::TransferWrite);
    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) {
                continue;
            }
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsCountBuffer(), SwDependency::BufferDepType::TransferWrite);
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::TransferWrite);
            deps.mWriteBuffers.emplace_back(&batch.getPreCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        }
    }
    mPasses[SwPass::Type::CullReset] = SwPass(SwPass::Type::CullReset, deps, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mCullResources.mResetPipelineBundle.getBindPoint(), mCullResources.mResetPipelineBundle.getRawPipeline());
        cmd.fillBuffer(sRendererContext.mStats->mRenderInstancesCountBuffer.getRawBuffer(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mSceneVisibleRenderInstancesInstanceIndexBuffer.getRawBuffer(), 0, vk::WholeSize, UINT32_MAX);
        for (auto& batchType : mBatchTypes | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.fillBuffer(batch.getPostCullRenderItemsCountBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                cmd.fillBuffer(batch.getPostCullRenderItemsBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                const std::uint32_t renderItemsCount = static_cast<std::uint32_t>(batch.getRenderItems().size());
                mCullResources.mResetPushConstants.mPreCullRenderItemsBuffer = batch.getPreCullRenderItemsBuffer().getDeviceAddress().value();
                mCullResources.mResetPushConstants.mPreCullRenderItemsLimit = renderItemsCount;
                cmd.pushConstants<SwCull::ResetPC>(
                    mCullResources.mResetPipelineBundle.getRawLayout(), SwCull::ResetPC::sStages, 0, mCullResources.mResetPushConstants
                );
                cmd.dispatch(SwHelper::fastDivCeil(renderItemsCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    deps.clear();

    // Cull Depth Pyramid
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::ComputeStorageRead);
    deps.mReadImages.emplace_back(&mCullResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
    deps.mWriteImages.emplace_back(&mCullResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageReadWrite);
    mPasses[SwPass::Type::CullDepthPyramid] = SwPass(SwPass::Type::CullDepthPyramid, deps, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mCullResources.mDepthPyramidPipelineBundle.getBindPoint(), mCullResources.mDepthPyramidPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            mCullResources.mDepthPyramidPipelineBundle.getRawLayout(),
            0,
            mCullResources.mDepthPyramidDescriptorSet.getRawSet(),
            nullptr
        );
        mCullResources.mDepthPyramidPushConstants.mReadFromFull = true;
        mCullResources.mDepthPyramidPushConstants.mLevel = 0;
        cmd.pushConstants<SwCull::DepthPyramidPC>(
            mCullResources.mDepthPyramidPipelineBundle.getRawLayout(), SwCull::DepthPyramidPC::sStages, 0, mCullResources.mDepthPyramidPushConstants
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(sRendererContext.mSwapchain->getDepthImage().getExtent().width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(sRendererContext.mSwapchain->getDepthImage().getExtent().height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
        mCullResources.mDepthPyramidPushConstants.mReadFromFull = false;
        for (std::uint32_t i = 0; i < mCullResources.mDepthPyramidLevels - 1; i++) {
            cmd.bindDescriptorSets(
                mCullResources.mDepthPyramidPipelineBundle.getBindPoint(),
                mCullResources.mDepthPyramidPipelineBundle.getRawLayout(),
                0,
                mCullResources.mDepthPyramidDescriptorSet.getRawSet(),
                nullptr
            );
            mCullResources.mDepthPyramidPushConstants.mLevel = i;
            cmd.pushConstants<SwCull::DepthPyramidPC>(
                mCullResources.mDepthPyramidPipelineBundle.getRawLayout(), SwCull::DepthPyramidPC::sStages, 0, mCullResources.mDepthPyramidPushConstants
            );
            mCullResources.mDepthPyramidImage.emitBarrier(
                cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
            );
            cmd.dispatch(
                SwHelper::fastDivCeil(mCullResources.mDepthPyramidImage.getExtent().width >> i, SwRenderer::MAX_2D_WORKGROUP_THREADS),
                SwHelper::fastDivCeil(mCullResources.mDepthPyramidImage.getExtent().height >> i, SwRenderer::MAX_2D_WORKGROUP_THREADS),
                1
            );
        }
    });
    deps.clear();

    // Cull Work
    deps.mReadImages.emplace_back(&mCullResources.mDepthPyramidImage, SwDependency::ImageDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneBoundsBuffer, SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneNodeTransformsBuffer, SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneInstancesBuffer, SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    deps.mReadBuffers.emplace_back(&mCamera.getFrustumBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    deps.mWriteBuffers.emplace_back(&sRendererContext.mStats->mRenderInstancesCountBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    deps.mWriteBuffers.emplace_back(&mSceneVisibleRenderInstancesInstanceIndexBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) {
                continue;
            }
            deps.mReadBuffers.emplace_back(&batch.getRenderInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            deps.mReadBuffers.emplace_back(&batch.getPreCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        }
    }
    mPasses[SwPass::Type::CullWork] = SwPass(SwPass::Type::CullWork, deps, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mCullResources.mWorkPipelineBundle.getBindPoint(), mCullResources.mWorkPipelineBundle.getRawPipeline());
        mCullResources.mWorkPushConstants.mPerFrameBuffer = sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
        // r->mScene.mCuller.mDepthPyramidImage.transition(
        //     cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        //);
        for (auto& batchType : mBatchTypes | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.bindDescriptorSets(
                    mCullResources.mWorkPipelineBundle.getBindPoint(),
                    mCullResources.mWorkPipelineBundle.getRawLayout(),
                    0,
                    mCullResources.mWorkDescriptorSet.getRawSet(),
                    nullptr
                );
                mCullResources.mWorkPushConstants.mPreCullRenderItemsBuffer = batch.getPreCullRenderItemsBuffer().getDeviceAddress().value();
                mCullResources.mWorkPushConstants.mRenderInstancesBuffer = batch.getRenderInstancesBuffer().getDeviceAddress().value();
                mCullResources.mWorkPushConstants.mRenderInstancesLimit = batch.getRenderInstances().size();
                cmd.pushConstants<SwCull::WorkPC>(
                    mCullResources.mWorkPipelineBundle.getRawLayout(), SwCull::WorkPC::sStages, 0, mCullResources.mWorkPushConstants
                );
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRenderInstances().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    deps.clear();

    // Cull Compact
    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            if (batch.getRenderItems().empty()) {
                continue;
            }
            deps.mReadBuffers.emplace_back(&batch.getPreCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
            deps.mWriteBuffers.emplace_back(&batch.getPostCullRenderItemsCountBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        }
    }
    mPasses[SwPass::Type::CullCompact] = SwPass(SwPass::Type::CullCompact, deps, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mCullResources.mCompactPipelineBundle.getBindPoint(), mCullResources.mCompactPipelineBundle.getRawPipeline());
        for (auto& batchType : mBatchTypes | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                mCullResources.mCompactPushConstants.mPreCullRenderItemsBuffer = batch.getPreCullRenderItemsBuffer().getDeviceAddress().value();
                mCullResources.mCompactPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                mCullResources.mCompactPushConstants.mPostCullRenderItemsCountBuffer = batch.getPostCullRenderItemsCountBuffer().getDeviceAddress().value();
                mCullResources.mCompactPushConstants.mPreCullRenderItemsLimit = batch.getRenderItems().size();
                cmd.pushConstants<SwCull::CompactPC>(
                    mCullResources.mCompactPipelineBundle.getRawLayout(), SwCull::CompactPC::sStages, 0, mCullResources.mCompactPushConstants
                );
                cmd.dispatch(SwHelper::fastDivCeil(batch.getRenderItems().size(), SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });
    deps.clear();
}

void SwScene::initializePickResources() {
    mPickResources.mReadbackBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        sizeof(SwPick::ReadbackData)
    );

    mPickResources.mReadbackDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute);
    mPickResources.mReadbackDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mPickResources.mReadbackDescriptorLayout);

    mPickResources.mDrawPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwPick::DrawPC::getRange());

    SwShader drawVertexShader = SwShaderFactory::createShader(PICK_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader drawFragmentShader = SwShaderFactory::createShader(PICK_DRAW_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    noBlendState.blendEnable = VK_FALSE;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getRawModule();
    drawPipelineOptions.mFragmentShader = drawFragmentShader.getRawModule();
    drawPipelineOptions.mLayout = mPickResources.mDrawPipelineLayout.getRawLayout();
    drawPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    drawPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    drawPipelineOptions.mCullMode = vk::CullModeFlagBits::eBack;
    drawPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    drawPipelineOptions.mMultisamplingEnabled = false;
    drawPipelineOptions.mSampleShadingEnabled = false;
    drawPipelineOptions.mColorAttachments = std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{vk::Format::eR32G32Uint, noBlendState}};
    drawPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    drawPipelineOptions.mDepthTestEnabled = true;
    drawPipelineOptions.mDepthWriteEnabled = true;
    drawPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    mPickResources.mDrawPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(drawPipelineOptions);

    mPickResources.mReadbackPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mPickResources.mReadbackDescriptorLayout.getRawLayout(), SwPick::ReadbackPC::getRange());

    SwShader workShader = SwShaderFactory::createShader(PICK_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mPickResources.mReadbackPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mPickResources.mReadbackPipelineLayout.getRawLayout()});

    mPickResources.mDrawPushConstants.mSceneVertexBuffer = mSceneVertexBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mSceneNodeTransformsBuffer = mSceneNodeTransformsBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mSceneInstancesBuffer = mSceneInstancesBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mSceneVisibleRenderInstancesInstanceIndexBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mPostCullRenderItemsBuffer = 0;

    mPickResources.mReadbackPushConstants.mPickerBuffer = mPickResources.mReadbackBuffer.getDeviceAddress().value();

    onResizeInitializePickResources();
}

void SwScene::onResizeInitializePickResources() {
    vk::Extent3D imageExtent = vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1};
    mPickResources.mReadbackImage = SwImageFactory::createColorImage2D(
        nullptr,
        vk::Format::eR32G32Uint,
        imageExtent,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        false
    );

    mPickResources.mDepthImage =
        SwImageFactory::createDepthImage2D(nullptr, SwSwapchain::DEPTH_FORMAT, imageExtent, vk::ImageUsageFlagBits::eDepthStencilAttachment);

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mPickResources.mReadbackImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );
        mPickResources.mDepthImage.emitTransition(
            cmd,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        );
    });

    mPickResources.mReadbackDescriptorSet.writeImage(
        0, mPickResources.mReadbackImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mPickResources.mReadbackDescriptorSet.pushWrites();
}

void SwScene::initializePickPasses() {
    SwDependency deps;

    // Pick Draw
    deps.mWriteImages.emplace_back(&mPickResources.mReadbackImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mPickResources.mDepthImage, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&mPickResources.mDepthImage, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadBuffers.emplace_back(&mSceneVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneNodeTransformsBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneInstancesBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneVisibleRenderInstancesInstanceIndexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneIndexBuffer, SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mBatchTypes | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            deps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mPasses[SwPass::Type::PickDraw] = SwPass(SwPass::Type::PickDraw, deps, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = mPickResources.mReadbackImage.generateRenderingAttachment();
        vk::RenderingAttachmentInfo depthAttachment = mPickResources.mDepthImage.generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mPickResources.mDrawPipelineBundle.getBindPoint(), mPickResources.mDrawPipelineBundle.getRawPipeline());
        SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
        cmd.bindIndexBuffer(mSceneIndexBuffer.getRawBuffer(), 0, vk::IndexType::eUint32);
        for (auto& batchType : mBatchTypes | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                mPickResources.mDrawPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                cmd.pushConstants<SwPick::DrawPC>(
                    batch.getGraphicsPipelineBundle().getRawLayout(), SwPick::DrawPC::sStages, 0, mPickResources.mDrawPushConstants
                );
                cmd.drawIndexedIndirectCount(
                    batch.getPostCullRenderItemsBuffer().getRawBuffer(),
                    0,
                    batch.getPostCullRenderItemsCountBuffer().getRawBuffer(),
                    0,
                    DRAW_MAX_RENDER_ITEMS,
                    sizeof(SwRenderItem)
                );
            }
        }

        cmd.endRendering();
    });
    deps.clear();

    // Pick Readback
    deps.mReadImages.emplace_back(&mPickResources.mReadbackImage, SwDependency::ImageDepType::ShaderSampledRead);
    deps.mWriteBuffers.emplace_back(&mPickResources.mReadbackBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    mPasses[SwPass::Type::PickReadback] = SwPass(SwPass::Type::PickReadback, deps, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mPickResources.mReadbackPipelineBundle.getBindPoint(), mPickResources.mReadbackPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mPickResources.mReadbackPipelineBundle.getBindPoint(),
            mPickResources.mReadbackPipelineBundle.getRawLayout(),
            0,
            mPickResources.mReadbackDescriptorSet.getRawSet(),
            nullptr
        );
        cmd.pushConstants<SwPick::ReadbackPC>(
            mPickResources.mReadbackPipelineBundle.getRawLayout(), SwPick::ReadbackPC::sStages, 0, mPickResources.mReadbackPushConstants
        );
        cmd.dispatch(1, 1, 1);
    });
    deps.clear();

    // Pick Work
    deps.mReadBuffers.emplace_back(&mPickResources.mReadbackBuffer, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
    mPasses[SwPass::Type::PickWork] = SwPass(SwPass::Type::PickWork, deps, [&](vk::CommandBuffer cmd) {
        glm::uvec2 read(0);
        std::memcpy(
            glm::value_ptr(read),
            static_cast<char*>(mPickResources.mReadbackBuffer.getMappedPointer()) + sizeof(SwPick::ReadbackData::mCoords),
            sizeof(SwPick::ReadbackData::mRead)
        );

        if (read.x == 0 || read.y == 0) {
            mPickResources.mClickedInstance = nullptr;
            return;
        }
        std::uint32_t modelId = read.x - 1;
        if (mAssets.contains(modelId)) {
            mPickResources.mClickedInstance = nullptr;
            return;
        }
        SwAsset& selectedAsset = mAssets[modelId];

        std::uint32_t localInstanceIndex = (read.y - 1) - selectedAsset.mFirstInstanceInScene;
        mPickResources.mClickedInstance = &selectedAsset.getInstances()[localInstanceIndex];
    });
    deps.clear();
}

void SwScene::changePickOperation() {
    switch (mPickResources.mImguizmoOperation) {
        case ImGuizmo::TRANSLATE:
            mPickResources.mImguizmoOperation = ImGuizmo::ROTATE;
            break;
        case ImGuizmo::ROTATE:
            mPickResources.mImguizmoOperation = ImGuizmo::SCALEU;
            break;
        case ImGuizmo::SCALEU:
            mPickResources.mImguizmoOperation = ImGuizmo::TRANSLATE;
            break;
        default:
            mPickResources.mImguizmoOperation = ImGuizmo::TRANSLATE;
    }
}

void SwScene::generatePickFrame() {
    if (mPickResources.mClickedInstance == nullptr) return;

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetGizmoSizeClipSpace(SwPick::PICK_IMGUIZMO_SIZE);

    ImGuizmo::SetRect(
        0,
        0,
        static_cast<float>(sRendererContext.mSwapchain->getWindowExtent().width),
        static_cast<float>(sRendererContext.mSwapchain->getWindowExtent().height)
    );

    mPickResources.mClickedInstance->getDataAddress()->mTransformMatrix;
    ImGuizmo::Manipulate(
        glm::value_ptr(mCamera.getPerspective().mView),
        glm::value_ptr(mCamera.getPerspective().mProj),
        mPickResources.mImguizmoOperation,
        ImGuizmo::WORLD,
        glm::value_ptr(mPickResources.mClickedInstance->getDataAddress()->mTransformMatrix)
    );

    if (ImGuizmo::IsUsing()) {
        mAssets[mPickResources.mClickedInstance->getAssetId()].setReloadInstancesFlag(true);
    }
}

void SwScene::initializeSkyboxResources() {
    mSkyboxResources.mWorkSampler = SwSamplerFactory::createSampler(vk::SamplerCreateInfo());

    mSkyboxResources.mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eCombinedImageSampler, 1}}, vk::ShaderStageFlagBits::eFragment);
    mSkyboxResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mSkyboxResources.mWorkDescriptorLayout);

    mSkyboxResources.mWorkPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mSkyboxResources.mWorkDescriptorLayout.getRawLayout(), SwSkybox::WorkPC::getRange());

    SwShader skyboxVertexShader = SwShaderFactory::createShader(SKYBOX_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader skyboxFragmentShader = SwShaderFactory::createShader(SKYBOX_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

    vk::PipelineColorBlendAttachmentState skyboxBlendAttachment{};
    skyboxBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    skyboxBlendAttachment.blendEnable = VK_TRUE;
    skyboxBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOneMinusDstAlpha;
    skyboxBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
    skyboxBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    skyboxBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    skyboxBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    skyboxBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions skyboxPipelineOptions;
    skyboxPipelineOptions.mVertexShader = skyboxVertexShader.getRawModule();
    skyboxPipelineOptions.mFragmentShader = skyboxFragmentShader.getRawModule();
    skyboxPipelineOptions.mLayout = mSkyboxResources.mWorkPipelineLayout.getRawLayout();
    skyboxPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    skyboxPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    skyboxPipelineOptions.mCullMode = vk::CullModeFlagBits::eBack;
    skyboxPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    skyboxPipelineOptions.mMultisamplingEnabled = false;
    skyboxPipelineOptions.mSampleShadingEnabled = false;
    skyboxPipelineOptions.mColorAttachments =
        std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{SwSwapchain::DRAW_FORMAT, skyboxBlendAttachment}};
    skyboxPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    skyboxPipelineOptions.mDepthTestEnabled = false;
    skyboxPipelineOptions.mDepthWriteEnabled = false;
    skyboxPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    mSkyboxResources.mWorkPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(skyboxPipelineOptions);

    const std::uint32_t skyboxVertexSize = static_cast<std::uint32_t>(mSkyboxResources.mWorkVertices.size() * sizeof(float));
    mSkyboxResources.mWorkVertexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        skyboxVertexSize
    );

    SwStagingBuffer skyboxVertexStagingBuffer = SwBufferFactory::createStagingBuffer(skyboxVertexSize);
    std::memcpy(skyboxVertexStagingBuffer.getMappedPointer(), mSkyboxResources.mWorkVertices.data(), skyboxVertexSize);

    vk::BufferCopy skyboxVertexCopy{};
    skyboxVertexCopy.dstOffset = 0;
    skyboxVertexCopy.srcOffset = 0;
    skyboxVertexCopy.size = skyboxVertexSize;

    sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        skyboxVertexStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        mSkyboxResources.mWorkVertexBuffer.copyFrom(cmd, skyboxVertexStagingBuffer, skyboxVertexCopy, skyboxVertexCopy.size);
        mSkyboxResources.mWorkVertexBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);
    });

    mSkyboxResources.mWorkPushConstants.mWorkVertexBuffer = mSkyboxResources.mWorkVertexBuffer.getDeviceAddress().value();

    onUpdateInitializeSkyboxResources();
}

void SwScene::onUpdateInitializeSkyboxResources() {
    if (!mSkyboxResources.mLoadFromDir.has_value()) {
        return;
    }

    std::vector<std::filesystem::path> skyboxImagePaths = {
        mSkyboxResources.mLoadFromDir.value() / "px.png",
        mSkyboxResources.mLoadFromDir.value() / "nx.png",
        mSkyboxResources.mLoadFromDir.value() / "py.png",
        mSkyboxResources.mLoadFromDir.value() / "ny.png",
        mSkyboxResources.mLoadFromDir.value() / "pz.png",
        mSkyboxResources.mLoadFromDir.value() / "nz.png"
    };
    std::vector<std::byte> skyboxImageData;

    int width = 0;
    int height = 0;
    int nrChannels = 0;
    for (auto& path : skyboxImagePaths) {
        int faceWidth = 0;
        int faceHeight = 0;
        int faceChannels = 0;
        if (unsigned char* data = stbi_load(path.string().c_str(), &faceWidth, &faceHeight, &faceChannels, 4)) {
            if (width == 0 && height == 0) {
                width = faceWidth;
                height = faceHeight;
            }
            const std::size_t imageSize = static_cast<std::size_t>(faceWidth) * static_cast<std::size_t>(faceHeight) * 4;
            const std::size_t offset = skyboxImageData.size();
            skyboxImageData.resize(offset + imageSize);
            std::memcpy(skyboxImageData.data() + offset, data, imageSize);
            stbi_image_free(data);
        }
    }
    if (width == 0 || height == 0 || skyboxImageData.empty()) {
        return;
    }

    mSkyboxResources.mWorkImage = SwImageFactory::createColorImageCubemap(
        skyboxImageData.data(),
        vk::Format::eR8G8B8A8Srgb,
        vk::Extent3D{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        vk::ImageUsageFlagBits::eSampled,
        true
    );

    mSkyboxResources.mWorkDescriptorSet.writeImage(
        0,
        mSkyboxResources.mWorkImage.getRawMainImageView(),
        mSkyboxResources.mWorkSampler.getRawSampler(),
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::DescriptorType::eCombinedImageSampler
    );
    mSkyboxResources.mWorkDescriptorSet.pushWrites();
}

void SwScene::initializeSkyboxPasses() {
    SwDependency deps;

    // Skybox
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&mSkyboxResources.mWorkImage, SwDependency::ImageDepType::ShaderSampledRead);
    deps.mReadBuffers.emplace_back(&mSkyboxResources.mWorkVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    mPasses[SwPass::Type::Skybox] = SwPass(SwPass::Type::Skybox, deps, [&](vk::CommandBuffer cmd) {
        const vk::RenderingAttachmentInfo colorAttachment = sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
        const vk::RenderingAttachmentInfo depthAttachment = sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mSkyboxResources.mWorkPipelineBundle.getBindPoint(), mSkyboxResources.mWorkPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mSkyboxResources.mWorkPipelineBundle.getBindPoint(),
            mSkyboxResources.mWorkPipelineBundle.getRawLayout(),
            0,
            mSkyboxResources.mWorkDescriptorSet.getRawSet(),
            nullptr
        );
        SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
        mSkyboxResources.mWorkPushConstants.mPerFrameBuffer = sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
        cmd.pushConstants<SwSkybox::WorkPC>(
            mSkyboxResources.mWorkPipelineBundle.getRawLayout(), SwSkybox::WorkPC::sStages, 0, mSkyboxResources.mWorkPushConstants
        );
        cmd.draw(SwSkybox::NUM_SKYBOX_VERTICES, 1, 0, 0);
        sRendererContext.mStats->mDrawCallCount++;

        cmd.endRendering();
    });
    deps.clear();
}

void SwScene::initializeWBOITResources() {
    mWBOITResources.mWorkDescriptorLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eFragment
    );
    mWBOITResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mWBOITResources.mWorkDescriptorLayout);

    mWBOITResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout(mWBOITResources.mWorkDescriptorLayout.getRawLayout(), nullptr);

    SwShader wboitVertexShader = SwShaderFactory::createShader(WBOIT_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader wboitFragmentShader = SwShaderFactory::createShader(WBOIT_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

    vk::PipelineColorBlendAttachmentState compositeBlendAttachment{};
    compositeBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    compositeBlendAttachment.blendEnable = VK_TRUE;
    compositeBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    compositeBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    compositeBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    compositeBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    compositeBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    compositeBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions wboitPipelineOptions;
    wboitPipelineOptions.mVertexShader = wboitVertexShader.getRawModule();
    wboitPipelineOptions.mFragmentShader = wboitFragmentShader.getRawModule();
    wboitPipelineOptions.mLayout = mWBOITResources.mWorkPipelineLayout.getRawLayout();
    wboitPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    wboitPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    wboitPipelineOptions.mCullMode = vk::CullModeFlagBits::eNone;
    wboitPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    wboitPipelineOptions.mMultisamplingEnabled = false;
    wboitPipelineOptions.mSampleShadingEnabled = false;
    wboitPipelineOptions.mColorAttachments =
        std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{SwSwapchain::DRAW_FORMAT, compositeBlendAttachment}};
    wboitPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    wboitPipelineOptions.mDepthTestEnabled = false;
    wboitPipelineOptions.mDepthWriteEnabled = false;
    wboitPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    mWBOITResources.mWorkPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(wboitPipelineOptions);

    onResizeInitializeWBOITResources();
}

void SwScene::onResizeInitializeWBOITResources() {
    mWBOITResources.mAccumImage = SwImageFactory::createColorImage2D(
        nullptr,
        SwSwapchain::DRAW_FORMAT,
        vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1},
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        false
    );
    mWBOITResources.mRvlImage = SwImageFactory::createColorImage2D(
        nullptr,
        SwWBOIT::RVL_FORMAT,
        vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1},
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        false,
        SwWBOIT::RVL_CLEAR_VALUE
    );

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mWBOITResources.mAccumImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead
        );
        mWBOITResources.mRvlImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead
        );
    });

    mWBOITResources.mWorkDescriptorSet.writeImage(
        0, mWBOITResources.mAccumImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mWBOITResources.mWorkDescriptorSet.writeImage(
        1, mWBOITResources.mRvlImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mWBOITResources.mWorkDescriptorSet.pushWrites();
}

void SwScene::initializeWBOITPasses() {
    SwDependency deps;

    // WBOIT Composite
    deps.mReadImages.emplace_back(&mWBOITResources.mAccumImage, SwDependency::ImageDepType::ShaderSampledRead);
    deps.mReadImages.emplace_back(&mWBOITResources.mRvlImage, SwDependency::ImageDepType::ShaderSampledRead);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    mPasses[SwPass::Type::WBOITComposite] = SwPass(SwPass::Type::WBOITComposite, deps, [&](vk::CommandBuffer cmd) {
        const vk::RenderingAttachmentInfo colorAttachment = sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachment, nullptr);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mWBOITResources.mWorkPipelineBundle.getBindPoint(), mWBOITResources.mWorkPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mWBOITResources.mWorkPipelineBundle.getBindPoint(),
            mWBOITResources.mWorkPipelineBundle.getRawLayout(),
            0,
            mWBOITResources.mWorkDescriptorSet.getRawSet(),
            nullptr
        );
        SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
        cmd.draw(SwSwapchain::NUM_FULLSCREEN_QUAD_VERTICES, 1, 0, 0);
        sRendererContext.mStats->mDrawCallCount++;

        cmd.endRendering();
    });
    deps.clear();
}

void SwScene::initializeGeometryResources() {
    mGeometryResources.mWorkPushConstants.mSceneVertexBuffer = mSceneVertexBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneMaterialConstantsBuffer = mSceneMaterialConstantsBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mSceneNodeTransformsBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneInstancesBuffer = mSceneInstancesBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mSceneVisibleRenderInstancesInstanceIndexBuffer.getDeviceAddress().value();
}

void SwScene::initializeGeometryPasses() {
    SwDependency deps;

    // Opaque and Masked
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadBuffers.emplace_back(&mSceneVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneMaterialConstantsBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneNodeTransformsBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneInstancesBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneVisibleRenderInstancesInstanceIndexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneIndexBuffer, SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mBatchTypes) {
        if (batchType.first != SwMaterial::Type::Opaque && batchType.first != SwMaterial::Type::Mask) {
            continue;
        }
        for (auto& batch : batchType.second | std::views::values) {
            deps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mPasses[SwPass::Type::GeometryOpaque] = SwPass(SwPass::Type::GeometryOpaque, deps, [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            sRendererContext.mSwapchain->getWindowExtent(),
            sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment(),
            sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batchType : mBatchTypes) {
            if (batchType.first != SwMaterial::Type::Opaque && batchType.first != SwMaterial::Type::Mask) {
                continue;
            }
            for (auto& batch : batchType.second | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.bindPipeline(batch.getGraphicsPipelineBundle().getBindPoint(), batch.getGraphicsPipelineBundle().getRawPipeline());
                SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
                cmd.bindIndexBuffer(mSceneIndexBuffer.getRawBuffer(), 0, vk::IndexType::eUint32);
                cmd.bindDescriptorSets(
                    batch.getGraphicsPipelineBundle().getBindPoint(),
                    batch.getGraphicsPipelineBundle().getRawLayout(),
                    0,
                    mSceneMaterialResourcesDescriptorSet.getRawSet(),
                    nullptr
                );
                mGeometryResources.mWorkPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                cmd.pushConstants<SwGeometry::WorkPC>(
                    batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mGeometryResources.mWorkPushConstants
                );
                cmd.drawIndexedIndirectCount(
                    batch.getPostCullRenderItemsBuffer().getRawBuffer(),
                    0,
                    batch.getPostCullRenderItemsCountBuffer().getRawBuffer(),
                    0,
                    DRAW_MAX_RENDER_ITEMS,
                    sizeof(SwRenderItem)
                );
                sRendererContext.mStats->mDrawCallCount++;
                sRendererContext.mStats->mPreCullRenderInstancesCount += batch.getRenderInstances().size();
            }
        }

        cmd.endRendering();
    });
    deps.clear();

    // Transparent
    deps.mWriteImages.emplace_back(&mWBOITResources.mAccumImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mWBOITResources.mRvlImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadBuffers.emplace_back(&mSceneVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneMaterialConstantsBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneNodeTransformsBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneInstancesBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneVisibleRenderInstancesInstanceIndexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mSceneIndexBuffer, SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mBatchTypes) {
        if (batchType.first != SwMaterial::Type::Transparent) {
            continue;
        }
        for (auto& batch : batchType.second | std::views::values) {
            deps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mPasses[SwPass::Type::GeometryTransparent] = SwPass(SwPass::Type::GeometryTransparent, deps, [&](vk::CommandBuffer cmd) {
        vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(
            sRendererContext.mSwapchain->getWindowExtent(),
            {mWBOITResources.mAccumImage.generateRenderingAttachment(), mWBOITResources.mRvlImage.generateRenderingAttachment()},
            sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batchType : mBatchTypes) {
            if (batchType.first != SwMaterial::Type::Transparent) {
                continue;
            }
            for (auto& batch : batchType.second | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                cmd.bindPipeline(batch.getGraphicsPipelineBundle().getBindPoint(), batch.getGraphicsPipelineBundle().getRawPipeline());
                SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
                cmd.bindIndexBuffer(mSceneIndexBuffer.getRawBuffer(), 0, vk::IndexType::eUint32);
                cmd.bindDescriptorSets(
                    batch.getGraphicsPipelineBundle().getBindPoint(),
                    batch.getGraphicsPipelineBundle().getRawLayout(),
                    0,
                    mSceneMaterialResourcesDescriptorSet.getRawSet(),
                    nullptr
                );
                mGeometryResources.mWorkPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                cmd.pushConstants<SwGeometry::WorkPC>(
                    batch.getGraphicsPipelineBundle().getRawLayout(), SwGeometry::WorkPC::sStages, 0, mGeometryResources.mWorkPushConstants
                );
                cmd.drawIndexedIndirectCount(
                    batch.getPostCullRenderItemsBuffer().getRawBuffer(),
                    0,
                    batch.getPostCullRenderItemsCountBuffer().getRawBuffer(),
                    0,
                    DRAW_MAX_RENDER_ITEMS,
                    sizeof(SwRenderItem)
                );
                sRendererContext.mStats->mDrawCallCount++;
                sRendererContext.mStats->mPreCullRenderInstancesCount += batch.getRenderInstances().size();
            }
        }
        cmd.endRendering();
    });
    deps.clear();
}

void SwScene::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

void SwScene::initialize() {
    mCamera.initialize();

    initializeMiscPasses();

    initializeSceneResources();

    initializeCullResources();
    initializeCullPasses();

    initializePickResources();
    initializePickPasses();

    initializeSkyboxResources();
    initializeSkyboxPasses();

    initializeWBOITResources();
    initializeWBOITPasses();

    initializeGeometryResources();
    initializeGeometryPasses();
}

void SwScene::resize() {
    mCullResources.mDepthPyramidImage.destroy();
    onResizeInitializeCullResources();

    mPickResources.mReadbackImage.destroy();
    mPickResources.mDepthImage.destroy();
    onResizeInitializePickResources();

    mWBOITResources.mRvlImage.destroy();
    mWBOITResources.mAccumImage.destroy();
    onResizeInitializeWBOITResources();
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
                batch.getPreCullRenderItemsBuffer().emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);
                batch.getPreCullRenderItemsBuffer().copyFrom(cmd, batch.getRenderItemsStagingBuffer(), renderItemsCopy, renderItemsCopy.size);
                batch.getPreCullRenderItemsBuffer().emitBarrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead);

                cmd.fillBuffer(batch.getRenderInstancesBuffer().getRawBuffer(), 0, vk::WholeSize, 0);
                batch.getRenderInstancesBuffer().emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);
                batch.getRenderInstancesBuffer().copyFrom(cmd, batch.getRenderInstancesStagingBuffer(), renderInstancesCopy, renderInstancesCopy.size);
                batch.getRenderInstancesBuffer().emitBarrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead);
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

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        // Wait for main vertex buffer to finish uploading
        mSceneVertexBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);
    });
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

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        // Wait for main index buffer to finish uploading
        mSceneIndexBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);
    });
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

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        // Wait for main material constants buffer to finish uploading
        mSceneMaterialConstantsBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);
    });
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

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        // Wait for main node transforms buffer to finish uploading
        mSceneNodeTransformsBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);
    });
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

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        // Wait for main bounds buffer to finish uploading
        mSceneBoundsBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead);
    });
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

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        // Wait for main instances buffer to finish uploading
        mSceneInstancesBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);
    });
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

void SwScene::draw() {}