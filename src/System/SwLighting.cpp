#include <Renderer/SwHelper.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwStagingRing.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwLighting.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

SwDescriptorLayout SwLighting::Resources::sShadowsConsumeDescriptorLayout{};

void SwLighting::Resources::init() {
    sShadowsConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "ShadowsConsumeDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, MAX_NUM_SHADOW_CASTERS},
            {1, vk::DescriptorType::eSampledImage, MAX_NUM_SHADOW_CASTERS},
            {2, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwLighting::Resources::cleanup() { sShadowsConsumeDescriptorLayout.destroy(); }

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::regenerateShadowsRcs() {
    if (mScene.getSceneRcs().empty()) return;

    const std::uint64_t rcsBytes = mScene.getSceneRcs().size() * sizeof(SwRenderCommand);
    const std::uint64_t shadowRisIndicesBytes = mScene.getSceneRis().size() * sizeof(std::uint32_t) * MAX_NUM_SHADOW_CASTERS;

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, rcsBytes, shadowRisIndicesBytes](vk::CommandBuffer cmd) {
        std::array<vk::BufferCopy, MAX_NUM_SHADOW_CASTERS> shadowRcsCopies;
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            shadowRcsCopies[i] = vk::BufferCopy{0, i * rcsBytes, rcsBytes};
        }
        mResources.mShadowsRcsBuffer.copyFrom(cmd, mScene.getSceneInitialRcsBuffer(), shadowRcsCopies);

        mResources.mShadowsRisIndicesBuffer.ensureCapacity(cmd, shadowRisIndicesBytes);
    });
}

void SwLighting::System::initializeResources() {
    mResources.mShadowsRcsBuffer = SwBufferFactory::createAllocatedBuffer(
        "ShadowsRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, LIGHTING_INITIAL_SHADOWS_RENDER_COMMANDS_BUFFER_SIZE, true
    );
    mResources.mShadowsRisIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "ShadowsRisIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, LIGHTING_INITIAL_SHADOWS_RENDER_ITEMS_INDICES_BUFFER_SIZE, true
    );

    mResources.mClustersBuffer = SwBufferFactory::createAllocatedBuffer(
        "ClustersBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, LIGHTING_INITIAL_CLUSTERS_BUFFER_SIZE, true, false
    );
    mResources.mClustersActiveBooleansBuffer = SwBufferFactory::createAllocatedBuffer(
        "ClustersActiveBooleansBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, LIGHTING_INITIAL_CLUSTERS_ACTIVE_BOOLEANS_BUFFER_SIZE, true, false
    );
    mResources.mClustersActiveIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "ClustersActiveIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, LIGHTING_INITIAL_CLUSTERS_ACTIVE_INDICES_BUFFER_SIZE, true, false
    );
    mResources.mClustersActiveCount =
        SwBufferFactory::createAllocatedBuffer("ClustersActiveCount", vk::BufferUsageFlagBits::eStorageBuffer, 0, sizeof(std::uint32_t), true, false);

    // Linear filtering gives hardware 2x2 PCF per SampleCmp tap.
    // Opaque-black border so 2D taps outside a frustum read as lit.
    auto makeComparisonSampler = [](const char* name, vk::SamplerAddressMode addressMode) {
        vk::SamplerCreateInfo info{};
        info.magFilter = vk::Filter::eLinear;
        info.minFilter = vk::Filter::eLinear;
        info.mipmapMode = vk::SamplerMipmapMode::eNearest;
        info.addressModeU = addressMode;
        info.addressModeV = addressMode;
        info.addressModeW = addressMode;
        info.minLod = 0.0f;
        info.maxLod = vk::LodClampNone;
        info.anisotropyEnable = vk::False;
        info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
        info.compareEnable = vk::True;
        info.compareOp = vk::CompareOp::eGreaterOrEqual;
        return SwSamplerFactory::createSampler(name, info);
    };

    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadows2DMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("Shadow2DMap{}", i),
            LIGHTING_SHADOWS_MAP_FORMAT,
            vk::Extent3D{LIGHTING_SHADOWS_2D_MAP_WIDTH_HEIGHT, LIGHTING_SHADOWS_2D_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowsCubeMaps[i] = SwImageFactory::createDepthImageCubemap(
            std::format("ShadowCubeMap{}", i),
            LIGHTING_SHADOWS_MAP_FORMAT,
            vk::Extent3D{LIGHTING_SHADOWS_CUBEMAP_WIDTH_HEIGHT, LIGHTING_SHADOWS_CUBEMAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            false
        );
    }

    mResources.mShadowsMapsSampler = makeComparisonSampler("ShadowsMapsSampler", vk::SamplerAddressMode::eClampToBorder);
    mResources.mShadowsMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("ShadowsMapsDescriptorSet", Resources::sShadowsConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowsMapsDescriptorSet.writeImage(
            0, mResources.mShadows2DMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowsMapsDescriptorSet.writeImage(
            1, mResources.mShadowsCubeMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mShadowsMapsDescriptorSet.writeSampler(2, mResources.mShadowsMapsSampler.getHandle());
    mResources.mShadowsMapsDescriptorSet.pushWrites();
    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            mResources.mShadows2DMaps[i].emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);
            mResources.mShadowsCubeMaps[i].emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);
        }
    });

    mResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout("ResetPipelineLayout", nullptr, SwLighting::ResetPC::getRange());
    SwShader resetShader = SwShaderFactory::createShader("ResetShaderModule", SwLighting::LIGHTING_RESET_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("ResetPipeline", {resetShader.getHandle(), mResources.mResetPipelineLayout.getHandle()});

    mResources.mClustersBuildPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersBuildPipelineLayout", nullptr, SwLighting::ClustersBuildPC::getRange());
    SwShader clustersBuildShader =
        SwShaderFactory::createShader("ClustersBuildShaderModule", SwLighting::LIGHTING_CLUSTERS_BUILD_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersBuildPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersBuildPipeline", {clustersBuildShader.getHandle(), mResources.mClustersBuildPipelineLayout.getHandle()}
    );

    mResources.mClustersMarkActiveDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "ClustersMarkActiveDescriptorLayout", {{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mClustersMarkActiveDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        "ClustersMarkActiveDescriptorSet", mResources.mClustersMarkActiveDescriptorLayout
    );
    mResources.mClustersMarkActiveDescriptorSet.writeImage(
        0, SwRenderer::sRendererContext.mSwapchain->getDepthImage().getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mClustersMarkActiveDescriptorSet.pushWrites();
    mResources.mClustersMarkActivePipelineLayout = SwPipelineFactory::createPipelineLayout(
        "ClustersMarkActivePipelineLayout", mResources.mClustersMarkActiveDescriptorLayout.getHandle(), SwLighting::ClustersMarkActivePC::getRange()
    );
    SwShader clustersMarkActiveShader = SwShaderFactory::createShader(
        "ClustersMarkActiveShaderModule", SwLighting::LIGHTING_CLUSTERS_MARK_ACTIVE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mClustersMarkActivePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersMarkActivePipeline", {clustersMarkActiveShader.getHandle(), mResources.mClustersMarkActivePipelineLayout.getHandle()}
    );

    mResources.mClustersCompactActivePipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersCompactActivePipelineLayout", nullptr, SwLighting::ClustersCompactActivePC::getRange());
    SwShader clustersCompactActiveShader = SwShaderFactory::createShader(
        "ClustersCompactActiveShaderModule", SwLighting::LIGHTING_CLUSTERS_COMPACT_ACTIVE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mClustersCompactActivePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersCompactActivePipeline", {clustersCompactActiveShader.getHandle(), mResources.mClustersCompactActivePipelineLayout.getHandle()}
    );

    mResources.mClustersCullPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersCullPipelineLayout", nullptr, SwLighting::ClustersCullPC::getRange());
    SwShader clustersCullShader =
        SwShaderFactory::createShader("ClustersCullShaderModule", SwLighting::LIGHTING_CLUSTERS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersCullPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersCullPipeline", {clustersCullShader.getHandle(), mResources.mClustersCullPipelineLayout.getHandle()}
    );

    mResources.mShadowsCullPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ShadowsCullPipelineLayout", nullptr, SwLighting::ShadowsCullPC::getRange());
    SwShader shadowsCullShader =
        SwShaderFactory::createShader("ShadowsCullShaderModule", SwLighting::LIGHTING_SHADOWS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowsCullPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ShadowsCullPipeline", {shadowsCullShader.getHandle(), mResources.mShadowsCullPipelineLayout.getHandle()}
    );

    mResources.mShadowsDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowsDrawPipelineLayout", nullptr, SwLighting::ShadowDrawPC::getRange());
    SwShader drawVertexShader =
        SwShaderFactory::createShader("ShadowsDrawVertexShaderModule", SwLighting::LIGHTING_SHADOWS_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.blendEnable = VK_FALSE;
    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getHandle();
    drawPipelineOptions.mFragmentShader = std::nullopt;
    drawPipelineOptions.mLayout = mResources.mShadowsDrawPipelineLayout.getHandle();
    drawPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    drawPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    drawPipelineOptions.mCullMode = vk::CullModeFlagBits::eFront;
    drawPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    drawPipelineOptions.mMultisamplingEnabled = false;
    drawPipelineOptions.mSampleShadingEnabled = false;
    drawPipelineOptions.mColorAttachments = {};
    drawPipelineOptions.mDepthFormat = LIGHTING_SHADOWS_MAP_FORMAT;
    drawPipelineOptions.mDepthTestEnabled = true;
    drawPipelineOptions.mDepthWriteEnabled = true;
    drawPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    drawPipelineOptions.mVertexEntryPoint = LIGHTING_SHADOWS_DRAW_OPAQUE_ENTRY_POINT;
    mResources.mShadowsDrawOpaquePipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowsDrawPipeline", drawPipelineOptions);
    drawPipelineOptions.mVertexEntryPoint = LIGHTING_SHADOWS_DRAW_MASKED_ENTRY_POINT;
    mResources.mShadowsDrawMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowsDrawPipeline", drawPipelineOptions);
}

void SwLighting::System::initializePasses() {
    // Reset
    mScene.insertPass(SwPass::Type::LightingReset, [&](vk::CommandBuffer cmd) {
        cmd.fillBuffer(mResources.mShadowsRcsBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersActiveBooleansBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersActiveIndicesBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersActiveCount.getHandle(), 0, vk::WholeSize, 0);
        auto& resetPipeline = mResources.mResetPipelineBundle;
        cmd.bindPipeline(resetPipeline.getBindPoint(), resetPipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ResetPC>(resetPipeline.getLayoutHandle(), SwLighting::ResetPC::sStages, 0, mResources.mResetPc);
        cmd.dispatch(SwHelper::fastDivCeil(mResources.mResetPc.mShadowsRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Clusters Build
    mScene.insertPass(SwPass::Type::LightingClustersBuild, [&](vk::CommandBuffer cmd) {
        auto& clustersBuildPipeline = mResources.mClustersBuildPipelineBundle;
        cmd.bindPipeline(clustersBuildPipeline.getBindPoint(), clustersBuildPipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ClustersBuildPC>(
            clustersBuildPipeline.getLayoutHandle(), SwLighting::ClustersBuildPC::sStages, 0, mResources.mClustersBuildPc
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(LIGHTING_CLUSTERS_DIMENSIONS.x, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(LIGHTING_CLUSTERS_DIMENSIONS.y, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(LIGHTING_CLUSTERS_DIMENSIONS.z, SwRenderer::MAX_3D_WORKGROUP_THREADS)
        );
        glm::perspectiveRH_ZO
    });

    // Clusters Mark Active
    mScene.insertPass(SwPass::Type::LightingClustersMarkActive, [&](vk::CommandBuffer cmd) {
        auto& clustersMarkActivePipeline = mResources.mClustersMarkActivePipelineBundle;
        cmd.bindPipeline(clustersMarkActivePipeline.getBindPoint(), clustersMarkActivePipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ClustersMarkActivePC>(
            clustersMarkActivePipeline.getLayoutHandle(), SwLighting::ClustersMarkActivePC::sStages, 0, mResources.mClustersMarkActivePc
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(SwRenderer::sRendererContext.mSwapchain->getDepthImage().getExtent().width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(SwRenderer::sRendererContext.mSwapchain->getDepthImage().getExtent().height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
    });

    // Clusters Compact Active
    mScene.insertPass(SwPass::Type::LightingClustersCompactActive, [&](vk::CommandBuffer cmd) {});

    // Clusters Cull
    mScene.insertPass(SwPass::Type::LightingClustersCull, [&](vk::CommandBuffer cmd) {});

    // Shadows Cull
    mScene.insertPass(SwPass::Type::LightingShadowsCull, [&](vk::CommandBuffer cmd) {});

    // Shadows Draw
    mScene.insertPass(SwPass::Type::LightingShadowsDraw, [&](vk::CommandBuffer cmd) {});
}

void SwLighting::System::refresh() {
    mResources.mClustersMarkActiveDescriptorSet.writeImage(
        0, SwRenderer::sRendererContext.mSwapchain->getDepthImage().getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mClustersMarkActiveDescriptorSet.pushWrites();

    SwSystem::refresh();
}

void SwLighting::System::refreshDependencies() {
    // Reset
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveBooleansBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveCount, SwDependency::BufferDepType::TransferWrite);
    }

    // Clusters Build
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersBuild].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mWriteBuffers.emplace_back(&mResources.mClustersBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Clusters Mark Active
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersMarkActive].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadBuffers.emplace_back(&mResources.mClustersBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveBooleansBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Clusters Compact Active
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersCompactActive].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mResources.mClustersActiveBooleansBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveIndicesBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mReadBuffers.emplace_back(&mResources.mClustersActiveCount, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveCount, SwDependency::BufferDepType::ComputeStorageReadWrite);
    }

    // Clusters Cull
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mScene.getSceneLightsInfoBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Shadows Cull
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mReadBuffers.emplace_back(&mScene.getSceneLightsInfoBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }

    // Shadows Draw
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsDraw].getDeps();
        d.clear();
        for (std::size_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            d.mWriteImages.emplace_back(&mResources.mShadows2DMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
        }
        d.mReadBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    }
}

void SwLighting::System::refreshPushConstants() {
    mResources.mResetPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer.getDeviceAddress().value();
    mResources.mResetPc.mShadowsRcsLimit = SwRenderer::sRendererContext.mScene->getSceneRcs().size() * MAX_NUM_SHADOW_CASTERS;

    mResources.mClustersBuildPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mClustersBuildPc.mClustersBuffer = mResources.mClustersBuffer.getDeviceAddress().value();
    mResources.mClustersBuildPc.mInvProj = glm::inverse(SwRenderer::sRendererContext.mScene->getCamera().getPerspective().getProjVk());
    mResources.mClustersBuildPc.mTargetSize =
        glm::uvec2(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().width, SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().height);

    mResources.mClustersMarkActivePc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mClustersMarkActivePc.mClustersBuffer = mResources.mClustersBuffer.getDeviceAddress().value();
    mResources.mClustersMarkActivePc.mClustersActiveBooleansBuffer = mResources.mClustersActiveBooleansBuffer.getDeviceAddress().value();

    mResources.mClustersCompactActivePc.mClustersActiveBooleansBuffer = mResources.mClustersActiveBooleansBuffer.getDeviceAddress().value();
    mResources.mClustersCompactActivePc.mClustersActiveIndicesBuffer = mResources.mClustersActiveIndicesBuffer.getDeviceAddress().value();
    mResources.mClustersCompactActivePc.mClustersActiveCount = mResources.mClustersActiveCount.getDeviceAddress().value();

    mResources.mClustersCullPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneLightsInfoBuffer = mScene.getSceneLightsInfoBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();

    mResources.mShadowsCullPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer.getDeviceAddress().value();
    mResources.mShadowsCullPc.mShadowsRisIndicesBuffer = mResources.mShadowsRisIndicesBuffer.getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneLightsInfoBuffer = mScene.getSceneLightsInfoBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneBoundsBuffer = SwRenderer::sRendererContext.mScene->getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mShadowsRisLimit = SwRenderer::sRendererContext.mScene->getSceneRis().size() * MAX_NUM_SHADOW_CASTERS;

    mResources.mShadowsDrawPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer.getDeviceAddress().value();
    mResources.mShadowsDrawPc.mShadowsRisIndicesBuffer = mResources.mShadowsRisIndicesBuffer.getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneLightsInfoBuffer = mScene.getSceneLightsInfoBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneVertexBuffer = SwRenderer::sRendererContext.mScene->getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneMaterialConstantsBuffer = SwRenderer::sRendererContext.mScene->getSceneMaterialConstantsBuffer().getDeviceAddress().value();
}