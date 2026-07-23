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
    if (mScene.getRcs().empty()) return;

    const std::uint64_t rcsBytes = mScene.getRcs().size() * sizeof(SwRenderCommand);
    const std::uint64_t shadowRisIndicesBytes = mScene.getRis().size() * sizeof(std::uint32_t) * MAX_NUM_SHADOW_CASTERS;

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, rcsBytes, shadowRisIndicesBytes](vk::CommandBuffer cmd) {
        std::array<vk::BufferCopy, MAX_NUM_SHADOW_CASTERS> shadowRcsCopies;
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            shadowRcsCopies[i] = vk::BufferCopy{0, i * rcsBytes, rcsBytes};
        }
        mResources.mShadowsRcsBuffer.copyFrom(cmd, mScene.getInitialRcsBuffer(), shadowRcsCopies);

        mResources.mShadowsRisIndicesBuffer.ensureCapacity(cmd, shadowRisIndicesBytes);
    });
}

void SwLighting::System::initializeResources() {
    mResources.mLitIndicesBuffer =
        SwBufferFactory::createAllocatedBuffer("LitIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mResources.mShadowCastsIndicesBuffer =
        SwBufferFactory::createAllocatedBuffer("ShadowCastsIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SHADOW_CAST_INDICES_BUFFER_SIZE, true);
    mResources.mShadowsRcsBuffer =
        SwBufferFactory::createAllocatedBuffer("ShadowsRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mResources.mShadowsRisIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "ShadowsRisIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );

    mResources.mClustersBuffer =
        SwBufferFactory::createAllocatedBuffer("ClustersBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, CLUSTERS_BUFFER_SIZE, true, false);
    mResources.mClustersActiveBooleansBuffer = SwBufferFactory::createAllocatedBuffer(
        "ClustersActiveBooleansBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, CLUSTERS_ACTIVE_BOOLEANS_BUFFER_SIZE, true
    );
    mResources.mClustersActiveIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "ClustersActiveIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, CLUSTERS_ACTIVE_INDICES_BUFFER_SIZE, true
    );
    mResources.mClustersLightIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "ClustersLightIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mResources.mClustersLightCounts = SwBufferFactory::createAllocatedBuffer("ClustersLightCounts", vk::BufferUsageFlagBits::eStorageBuffer, 0, CLUSTERS_LIGHT_COUNTS_SIZE, true
    );

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
            SHADOWS_MAP_FORMAT,
            vk::Extent3D{SHADOWS_2D_MAP_WIDTH_HEIGHT, SHADOWS_2D_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
        mResources.mShadowsCubeMaps[i] = SwImageFactory::createDepthImageCubemap(
            std::format("ShadowCubeMap{}", i),
            SHADOWS_MAP_FORMAT,
            vk::Extent3D{SHADOWS_CUBEMAP_WIDTH_HEIGHT, SHADOWS_CUBEMAP_WIDTH_HEIGHT, 1},
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
    SwShader resetShader = SwShaderFactory::createShader("ResetShaderModule", SwLighting::RESET_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("ResetPipeline", {resetShader.getHandle(), mResources.mResetPipelineLayout.getHandle()});

    mResources.mClustersBuildPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersBuildPipelineLayout", nullptr, SwLighting::ClustersBuildPC::getRange());
    SwShader clustersBuildShader =
        SwShaderFactory::createShader("ClustersBuildShaderModule", SwLighting::CLUSTERS_BUILD_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
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
    SwShader clustersMarkActiveShader =
        SwShaderFactory::createShader("ClustersMarkActiveShaderModule", SwLighting::CLUSTERS_MARK_ACTIVE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersMarkActivePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersMarkActivePipeline", {clustersMarkActiveShader.getHandle(), mResources.mClustersMarkActivePipelineLayout.getHandle()}
    );

    mResources.mClustersCompactActivePipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersCompactActivePipelineLayout", nullptr, SwLighting::ClustersCompactActivePC::getRange());
    SwShader clustersCompactActiveShader =
        SwShaderFactory::createShader("ClustersCompactActiveShaderModule", SwLighting::CLUSTERS_COMPACT_ACTIVE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersCompactActivePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersCompactActivePipeline", {clustersCompactActiveShader.getHandle(), mResources.mClustersCompactActivePipelineLayout.getHandle()}
    );

    mResources.mLightsCullPipelineLayout = SwPipelineFactory::createPipelineLayout("LightsCullPipelineLayout", nullptr, SwLighting::LightsCullPC::getRange());
    SwShader lightsCullShader = SwShaderFactory::createShader("LightsCullShaderModule", SwLighting::LIGHTS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mLightsCullPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("LightsCullPipeline", {lightsCullShader.getHandle(), mResources.mLightsCullPipelineLayout.getHandle()});

    mResources.mClustersLightSelectPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersLightSelectPipelineLayout", nullptr, SwLighting::ClustersLightSelectPC::getRange());
    SwShader clustersLightSelectShader =
        SwShaderFactory::createShader("ClustersLightSelectShaderModule", SwLighting::CLUSTERS_LIGHT_SELECT_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersLightSelectPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersLightSelectPipeline", {clustersLightSelectShader.getHandle(), mResources.mClustersLightSelectPipelineLayout.getHandle()}
    );

    mResources.mShadowsCullPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ShadowsCullPipelineLayout", nullptr, SwLighting::ShadowsCullPC::getRange());
    SwShader shadowsCullShader =
        SwShaderFactory::createShader("ShadowsCullShaderModule", SwLighting::SHADOWS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowsCullPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ShadowsCullPipeline", {shadowsCullShader.getHandle(), mResources.mShadowsCullPipelineLayout.getHandle()}
    );

    mResources.mShadowsDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowsDrawPipelineLayout", nullptr, SwLighting::ShadowDrawPC::getRange());
    SwShader drawVertexShader =
        SwShaderFactory::createShader("ShadowsDrawVertexShaderModule", SwLighting::SHADOWS_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
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
    drawPipelineOptions.mDepthFormat = SHADOWS_MAP_FORMAT;
    drawPipelineOptions.mDepthTestEnabled = true;
    drawPipelineOptions.mDepthWriteEnabled = true;
    drawPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    drawPipelineOptions.mVertexEntryPoint = SHADOWS_DRAW_OPAQUE_ENTRY_POINT;
    mResources.mShadowsDrawOpaquePipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowsDrawPipeline", drawPipelineOptions);
    drawPipelineOptions.mVertexEntryPoint = SHADOWS_DRAW_MASKED_ENTRY_POINT;
    mResources.mShadowsDrawMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowsDrawPipeline", drawPipelineOptions);
}

void SwLighting::System::initializePasses() {
    // Reset
    mScene.insertPass(SwPass::Type::LightingReset, [&](vk::CommandBuffer cmd) {
        cmd.fillBuffer(mResources.mShadowsRcsBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mLitIndicesBuffer.getHandle(), 0, sizeof(std::uint32_t), 0);
        cmd.fillBuffer(mResources.mClustersActiveBooleansBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersActiveIndicesBuffer.getHandle(), 0, vk::WholeSize, 0);
        auto& resetPipeline = mResources.mResetPipelineBundle;
        cmd.bindPipeline(resetPipeline.getBindPoint(), resetPipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ResetPC>(resetPipeline.getLayoutHandle(), SwLighting::ResetPC::sStages, 0, mResources.mResetPc);
        if (mResources.mResetPc.mShadowsRcsLimit == 0) return;
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
            SwHelper::fastDivCeil(CLUSTERS_DIMENSIONS.x, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(CLUSTERS_DIMENSIONS.y, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(CLUSTERS_DIMENSIONS.z, SwRenderer::MAX_3D_WORKGROUP_THREADS)
        );
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
    mScene.insertPass(SwPass::Type::LightingClustersCompactActive, [&](vk::CommandBuffer cmd) {
        auto& clustersCompactActivePipeline = mResources.mClustersCompactActivePipelineBundle;
        cmd.bindPipeline(clustersCompactActivePipeline.getBindPoint(), clustersCompactActivePipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ClustersCompactActivePC>(
            clustersCompactActivePipeline.getLayoutHandle(), SwLighting::ClustersCompactActivePC::sStages, 0, mResources.mClustersCompactActivePc
        );
        cmd.dispatch(SwHelper::fastDivCeil(NUM_CLUSTERS, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Lights Cull
    mScene.insertPass(SwPass::Type::LightingLightsCull, [&](vk::CommandBuffer cmd) {
        auto& lightsCullPipeline = mResources.mLightsCullPipelineBundle;
        cmd.bindPipeline(lightsCullPipeline.getBindPoint(), lightsCullPipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::LightsCullPC>(lightsCullPipeline.getLayoutHandle(), SwLighting::LightsCullPC::sStages, 0, mResources.mLightsCullPc);
        std::uint32_t numLights = mScene.getLightIds().size();
        if (numLights == 0) return;
        cmd.dispatch(SwHelper::fastDivCeil(numLights, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Clusters Light Select
    mScene.insertPass(SwPass::Type::LightingClustersLightSelect, [&](vk::CommandBuffer cmd) {
        auto& clustersLightSelectPipeline = mResources.mClustersLightSelectPipelineBundle;
        cmd.bindPipeline(clustersLightSelectPipeline.getBindPoint(), clustersLightSelectPipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ClustersLightSelectPC>(
            clustersLightSelectPipeline.getLayoutHandle(), SwLighting::ClustersLightSelectPC::sStages, 0, mResources.mClustersLightSelectPc
        );
        std::uint32_t numLights = mScene.getLightIds().size();
        if (numLights == 0) return;
        cmd.dispatch(
            SwHelper::fastDivCeil(NUM_CLUSTERS, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(numLights, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
    });

    // Shadows Select
    mScene.insertPass(SwPass::Type::LightingShadowsSelect, [&](vk::CommandBuffer cmd) {});

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

void SwLighting::System::refreshDataUsage() {
    // Reset
    {
        mResources.mResetPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer;
        mResources.mResetPc.mShadowsRcsLimit = SwRenderer::sRendererContext.mScene->getRcs().size() * MAX_NUM_SHADOW_CASTERS;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mResources.mLitIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveBooleansBuffer, SwDependency::BufferDepType::TransferWrite);
    }

    // Clusters Build
    {
        mResources.mClustersBuildPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
        mResources.mClustersBuildPc.mClustersBuffer = mResources.mClustersBuffer;
        mResources.mClustersBuildPc.mInvProj = glm::inverse(SwRenderer::sRendererContext.mScene->getCamera().getPerspective().getProjVk());
        mResources.mClustersBuildPc.mTargetSize =
            glm::uvec2(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().width, SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().height);

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersBuild].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mWriteBuffers.emplace_back(&mResources.mClustersBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Clusters Mark Active
    {
        mResources.mClustersMarkActivePc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
        mResources.mClustersMarkActivePc.mClustersBuffer = mResources.mClustersBuffer;
        mResources.mClustersMarkActivePc.mClustersActiveBooleansBuffer = mResources.mClustersActiveBooleansBuffer;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersMarkActive].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::ComputeShaderSampledRead);
        d.mReadBuffers.emplace_back(&mResources.mClustersBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveBooleansBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Clusters Compact Active
    {
        mResources.mClustersCompactActivePc.mClustersActiveBooleansBuffer = mResources.mClustersActiveBooleansBuffer;
        mResources.mClustersCompactActivePc.mClustersActiveIndicesBuffer = mResources.mClustersActiveIndicesBuffer;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersCompactActive].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mResources.mClustersActiveBooleansBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveIndicesBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
    }

    // Lights Cull
    {
        mResources.mLightsCullPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
        mResources.mLightsCullPc.mLightsBuffer = SwRenderer::sRendererContext.mScene->getLightsBuffer();
        mResources.mLightsCullPc.mNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getNodeTransformsBuffer();
        mResources.mLightsCullPc.mInstancesBuffer = SwRenderer::sRendererContext.mScene->getInstancesBuffer();
        mResources.mLightsCullPc.mLitIndicesBuffer = mResources.mLitIndicesBuffer;
        mResources.mLightsCullPc.mShadowCastsIndicesBuffer = mResources.mShadowCastsIndicesBuffer;
        mResources.mLightsCullPc.mLightsCount = SwRenderer::sRendererContext.mScene->getLightIds().size();

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingLightsCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mLitIndicesBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mReadBuffers.emplace_back(&mResources.mShadowCastsIndicesBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mLitIndicesBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowCastsIndicesBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
    }

    // Clusters Light Select
    {
        mResources.mClustersLightSelectPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
        mResources.mClustersLightSelectPc.mLightsBuffer = SwRenderer::sRendererContext.mScene->getLightsBuffer();
        mResources.mClustersLightSelectPc.mNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getNodeTransformsBuffer();
        mResources.mClustersLightSelectPc.mInstancesBuffer = SwRenderer::sRendererContext.mScene->getInstancesBuffer();
        mResources.mClustersLightSelectPc.mClustersActiveIndicesBuffer = mResources.mClustersActiveIndicesBuffer;
        mResources.mClustersLightSelectPc.mClustersLightIndicesBuffer = mResources.mClustersLightIndicesBuffer;
        mResources.mClustersLightSelectPc.mClustersLightCounts = mResources.mClustersLightCounts;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersLightSelect].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }

    // Shadows Cull
    {
        mResources.mShadowsCullPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer;
        mResources.mShadowsCullPc.mShadowsRisIndicesBuffer = mResources.mShadowsRisIndicesBuffer;
        mResources.mShadowsCullPc.mLightsBuffer = SwRenderer::sRendererContext.mScene->getLightsBuffer();
        mResources.mShadowsCullPc.mBoundsBuffer = SwRenderer::sRendererContext.mScene->getBoundsBuffer();
        mResources.mShadowsCullPc.mNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getNodeTransformsBuffer();
        mResources.mShadowsCullPc.mInstancesBuffer = SwRenderer::sRendererContext.mScene->getInstancesBuffer();
        mResources.mShadowsCullPc.mShadowsRisLimit = SwRenderer::sRendererContext.mScene->getRis().size() * MAX_NUM_SHADOW_CASTERS;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }

    // Shadows Draw
    {
        mResources.mShadowsDrawPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer;
        mResources.mShadowsDrawPc.mShadowsRisIndicesBuffer = mResources.mShadowsRisIndicesBuffer;
        mResources.mShadowsDrawPc.mLightsBuffer = SwRenderer::sRendererContext.mScene->getLightsBuffer();
        mResources.mShadowsDrawPc.mVertexBuffer = SwRenderer::sRendererContext.mScene->getVertexBuffer();
        mResources.mShadowsDrawPc.mNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getNodeTransformsBuffer();
        mResources.mShadowsDrawPc.mInstancesBuffer = SwRenderer::sRendererContext.mScene->getInstancesBuffer();
        mResources.mShadowsDrawPc.mMaterialConstantsBuffer = SwRenderer::sRendererContext.mScene->getMaterialConstantsBuffer();

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsDraw].getDeps();
        d.clear();
        for (std::size_t i = 0; i < MAX_NUM_SHADOW_CASTERS; i++) {
            d.mWriteImages.emplace_back(&mResources.mShadows2DMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
        }
        d.mReadBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    }
}