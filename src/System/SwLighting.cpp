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
            {0, vk::DescriptorType::eSampledImage, MAX_DIRECTIONAL_SHADOW_MAPS},
            {1, vk::DescriptorType::eSampledImage, MAX_POINT_SHADOW_MAPS},
            {2, vk::DescriptorType::eSampledImage, MAX_SPOT_SHADOW_MAPS},
            {3, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwLighting::Resources::cleanup() { sShadowsConsumeDescriptorLayout.destroy(); }

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::regenerateShadowsRcs() {
    if (mScene.getRcs().empty()) return;

    const std::uint64_t rcsBytes = mScene.getRcs().size() * sizeof(SwRenderCommand);
    const std::uint64_t shadowRisIndicesBytes = mScene.getRis().size() * sizeof(std::uint32_t) * MAX_NUM_SHADOW_VIEWS;

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, rcsBytes, shadowRisIndicesBytes](vk::CommandBuffer cmd) {
        std::array<vk::BufferCopy, MAX_NUM_SHADOW_VIEWS> shadowRcsCopies;
        for (std::uint32_t i = 0; i < MAX_NUM_SHADOW_VIEWS; i++) {
            shadowRcsCopies[i] = vk::BufferCopy{0, i * rcsBytes, rcsBytes};
        }
        mResources.mShadowsRcsBuffer.copyFrom(cmd, mScene.getInitialRcsBuffer(), shadowRcsCopies);

        mResources.mShadowsRisIndicesBuffer.ensureCapacity(cmd, shadowRisIndicesBytes);
    });
}

void SwLighting::System::initializeResources() {
    mResources.mLightsVisibleIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "LightsVisibleIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mResources.mShadowsViewsBuffer = SwBufferFactory::createAllocatedBuffer(
        "ShadowsViewsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SHADOWS_VIEWS_BUFFER_SIZE, true
    );
    mResources.mShadowsRcsBuffer =
        SwBufferFactory::createAllocatedBuffer("ShadowsRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true);
    mResources.mShadowsRisIndicesBuffer = SwBufferFactory::createAllocatedBuffer(
        "ShadowsRisIndicesBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, SwBufferFactory::INITIAL_BUFFER_SIZE, true
    );
    mResources.mShadowMapSlotsCount =
        SwBufferFactory::createAllocatedBuffer("ShadowMapSlotsCount", vk::BufferUsageFlagBits::eStorageBuffer, 0, SHADOW_MAP_SLOTS_COUNT_SIZE, true);

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
    mResources.mClustersLightCounts =
        SwBufferFactory::createAllocatedBuffer("ClustersLightCounts", vk::BufferUsageFlagBits::eStorageBuffer, 0, CLUSTERS_LIGHT_COUNTS_SIZE, true);
    mResources.mClustersLightOffsetsBuffer =
        SwBufferFactory::createAllocatedBuffer("ClustersLightOffsetsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, CLUSTERS_LIGHT_OFFSETS_SIZE, true);
    mResources.mClustersLightWriteCursorsBuffer = SwBufferFactory::createAllocatedBuffer(
        "ClustersLightWriteCursorsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, CLUSTERS_LIGHT_WRITE_CURSORS_SIZE, true
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

    for (std::uint32_t i = 0; i < MAX_DIRECTIONAL_SHADOW_MAPS; i++) {
        mResources.mDirectionalShadowMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("DirectionalShadowMap{}", i),
            SHADOWS_MAP_FORMAT,
            vk::Extent3D{SHADOWS_2D_MAP_WIDTH_HEIGHT, SHADOWS_2D_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    for (std::uint32_t i = 0; i < MAX_POINT_SHADOW_MAPS; i++) {
        mResources.mPointShadowMaps[i] = SwImageFactory::createDepthImageCubemap(
            std::format("PointShadowMap{}", i),
            SHADOWS_MAP_FORMAT,
            vk::Extent3D{SHADOWS_CUBEMAP_WIDTH_HEIGHT, SHADOWS_CUBEMAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            false
        );
    }
    for (std::uint32_t i = 0; i < MAX_SPOT_SHADOW_MAPS; i++) {
        mResources.mSpotShadowMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("SpotShadowMap{}", i),
            SHADOWS_MAP_FORMAT,
            vk::Extent3D{SHADOWS_2D_MAP_WIDTH_HEIGHT, SHADOWS_2D_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }

    mResources.mShadowsMapsSampler = makeComparisonSampler("ShadowsMapsSampler", vk::SamplerAddressMode::eClampToBorder);
    mResources.mShadowsMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("ShadowsMapsDescriptorSet", Resources::sShadowsConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < MAX_DIRECTIONAL_SHADOW_MAPS; i++) {
        mResources.mShadowsMapsDescriptorSet.writeImage(
            0, mResources.mDirectionalShadowMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    for (std::uint32_t i = 0; i < MAX_POINT_SHADOW_MAPS; i++) {
        mResources.mShadowsMapsDescriptorSet.writeImage(
            1, mResources.mPointShadowMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    for (std::uint32_t i = 0; i < MAX_SPOT_SHADOW_MAPS; i++) {
        mResources.mShadowsMapsDescriptorSet.writeImage(
            2, mResources.mSpotShadowMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mShadowsMapsDescriptorSet.writeSampler(3, mResources.mShadowsMapsSampler.getHandle());
    mResources.mShadowsMapsDescriptorSet.pushWrites();
    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < MAX_DIRECTIONAL_SHADOW_MAPS; i++)
            mResources.mDirectionalShadowMaps[i].emitTransition(
                cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral
            );
        for (std::uint32_t i = 0; i < MAX_POINT_SHADOW_MAPS; i++)
            mResources.mPointShadowMaps[i].emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);
        for (std::uint32_t i = 0; i < MAX_SPOT_SHADOW_MAPS; i++)
            mResources.mSpotShadowMaps[i].emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);
    });

    mResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout("ResetPipelineLayout", nullptr, ResetPC::getRange());
    SwShader resetShader = SwShaderFactory::createShader("ResetShaderModule", RESET_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mResetPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("ResetPipeline", {resetShader.getHandle(), mResources.mResetPipelineLayout.getHandle()});

    mResources.mClustersBuildPipelineLayout = SwPipelineFactory::createPipelineLayout("ClustersBuildPipelineLayout", nullptr, ClustersBuildPC::getRange());
    SwShader clustersBuildShader = SwShaderFactory::createShader("ClustersBuildShaderModule", CLUSTERS_BUILD_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersBuildPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersBuildPipeline", {clustersBuildShader.getHandle(), mResources.mClustersBuildPipelineLayout.getHandle()}
    );

    mResources.mClustersMarkActiveDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "ClustersMarkActiveDescriptorLayout", {{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mClustersMarkActiveDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        "ClustersMarkActiveDescriptorSet", mResources.mClustersMarkActiveDescriptorLayout
    );
    mResources.mClustersMarkActivePipelineLayout = SwPipelineFactory::createPipelineLayout(
        "ClustersMarkActivePipelineLayout", mResources.mClustersMarkActiveDescriptorLayout.getHandle(), ClustersMarkActivePC::getRange()
    );
    SwShader clustersMarkActiveShader =
        SwShaderFactory::createShader("ClustersMarkActiveShaderModule", CLUSTERS_MARK_ACTIVE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersMarkActivePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersMarkActivePipeline", {clustersMarkActiveShader.getHandle(), mResources.mClustersMarkActivePipelineLayout.getHandle()}
    );

    mResources.mClustersCompactActivePipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersCompactActivePipelineLayout", nullptr, ClustersCompactActivePC::getRange());
    SwShader clustersCompactActiveShader =
        SwShaderFactory::createShader("ClustersCompactActiveShaderModule", CLUSTERS_COMPACT_ACTIVE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersCompactActivePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersCompactActivePipeline", {clustersCompactActiveShader.getHandle(), mResources.mClustersCompactActivePipelineLayout.getHandle()}
    );

    mResources.mLightsCullPipelineLayout = SwPipelineFactory::createPipelineLayout("LightsCullPipelineLayout", nullptr, LightsCullPC::getRange());
    SwShader lightsCullShader = SwShaderFactory::createShader("LightsCullShaderModule", LIGHTS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mLightsCullPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("LightsCullPipeline", {lightsCullShader.getHandle(), mResources.mLightsCullPipelineLayout.getHandle()});

    mResources.mClustersLightCalcOffsetPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersLightCalcOffsetPipelineLayout", nullptr, ClustersLightCalcOffsetPC::getRange());
    SwShader clustersLightCalcOffsetShader =
        SwShaderFactory::createShader("ClustersLightCalcOffsetShaderModule", CLUSTERS_LIGHT_CALC_OFFSET_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersLightCalcOffsetPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersLightCalcOffsetPipeline", {clustersLightCalcOffsetShader.getHandle(), mResources.mClustersLightCalcOffsetPipelineLayout.getHandle()}
    );

    mResources.mClustersLightPrefixSumOffsetPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersLightPrefixSumOffsetPipelineLayout", nullptr, ClustersLightPrefixSumOffsetPC::getRange());
    SwShader clustersLightPrefixSumOffsetShader = SwShaderFactory::createShader(
        "ClustersLightPrefixSumOffsetShaderModule", CLUSTERS_LIGHT_PREFIX_SUM_OFFSET_SHADER_PATH, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mClustersLightPrefixSumOffsetPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersLightPrefixSumOffsetPipeline",
        {clustersLightPrefixSumOffsetShader.getHandle(), mResources.mClustersLightPrefixSumOffsetPipelineLayout.getHandle()}
    );

    mResources.mClustersLightSelectPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersLightSelectPipelineLayout", nullptr, ClustersLightSelectPC::getRange());
    SwShader clustersLightSelectShader =
        SwShaderFactory::createShader("ClustersLightSelectShaderModule", CLUSTERS_LIGHT_SELECT_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersLightSelectPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersLightSelectPipeline", {clustersLightSelectShader.getHandle(), mResources.mClustersLightSelectPipelineLayout.getHandle()}
    );

    mResources.mShadowsCullPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowsCullPipelineLayout", nullptr, ShadowsCullPC::getRange());
    SwShader shadowsCullShader = SwShaderFactory::createShader("ShadowsCullShaderModule", SHADOWS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowsCullPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ShadowsCullPipeline", {shadowsCullShader.getHandle(), mResources.mShadowsCullPipelineLayout.getHandle()}
    );

    mResources.mShadowsDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowsDrawPipelineLayout", nullptr, ShadowDrawPC::getRange());
    SwShader drawVertexShader =
        SwShaderFactory::createShader("ShadowsDrawVertexShaderModule", SHADOWS_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
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

    reInitializeOnResize();
}

void SwLighting::System::initializePasses() {
    // Reset
    mScene.insertPass(SwPass::Type::LightingReset, [&](vk::CommandBuffer cmd) {
        cmd.fillBuffer(mResources.mLightsVisibleIndicesBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mShadowsViewsBuffer.getHandle(), 0, vk::WholeSize, UINT32_MAX);
        cmd.fillBuffer(mResources.mShadowsRcsBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mShadowMapSlotsCount.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersActiveBooleansBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersActiveIndicesBuffer.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersLightCounts.getHandle(), 0, vk::WholeSize, 0);
        cmd.fillBuffer(mResources.mClustersLightWriteCursorsBuffer.getHandle(), 0, vk::WholeSize, 0);
        auto& resetPipeline = mResources.mResetPipelineBundle;
        cmd.bindPipeline(resetPipeline.getBindPoint(), resetPipeline.getPipelineHandle());
        cmd.pushConstants<ResetPC>(resetPipeline.getLayoutHandle(), ResetPC::sStages, 0, mResources.mResetPc);
        if (mResources.mResetPc.mShadowsRcsLimit == 0) return;
        cmd.dispatch(SwHelper::fastDivCeil(mResources.mResetPc.mShadowsRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Clusters Build
    mScene.insertPass(SwPass::Type::LightingClustersBuild, [&](vk::CommandBuffer cmd) {
        auto& clustersBuildPipeline = mResources.mClustersBuildPipelineBundle;
        cmd.bindPipeline(clustersBuildPipeline.getBindPoint(), clustersBuildPipeline.getPipelineHandle());
        cmd.pushConstants<ClustersBuildPC>(clustersBuildPipeline.getLayoutHandle(), ClustersBuildPC::sStages, 0, mResources.mClustersBuildPc);
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
        cmd.bindDescriptorSets(
            clustersMarkActivePipeline.getBindPoint(),
            clustersMarkActivePipeline.getLayoutHandle(),
            0,
            mResources.mClustersMarkActiveDescriptorSet.getHandle(),
            {}
        );
        cmd.pushConstants<ClustersMarkActivePC>(
            clustersMarkActivePipeline.getLayoutHandle(), ClustersMarkActivePC::sStages, 0, mResources.mClustersMarkActivePc
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
        cmd.pushConstants<ClustersCompactActivePC>(
            clustersCompactActivePipeline.getLayoutHandle(), ClustersCompactActivePC::sStages, 0, mResources.mClustersCompactActivePc
        );
        cmd.dispatch(SwHelper::fastDivCeil(NUM_CLUSTERS, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Lights Cull
    mScene.insertPass(SwPass::Type::LightingLightsCull, [&](vk::CommandBuffer cmd) {
        auto& lightsCullPipeline = mResources.mLightsCullPipelineBundle;
        cmd.bindPipeline(lightsCullPipeline.getBindPoint(), lightsCullPipeline.getPipelineHandle());
        cmd.pushConstants<LightsCullPC>(lightsCullPipeline.getLayoutHandle(), LightsCullPC::sStages, 0, mResources.mLightsCullPc);
        std::uint32_t numLights = mScene.getLightIds().size();
        if (numLights == 0) return;
        cmd.dispatch(SwHelper::fastDivCeil(numLights, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Clusters Light Calc Offset
    mScene.insertPass(SwPass::Type::LightingClustersLightCalcOffset, [&](vk::CommandBuffer cmd) {
        auto& clustersLightCalcOffsetPipeline = mResources.mClustersLightCalcOffsetPipelineBundle;
        cmd.bindPipeline(clustersLightCalcOffsetPipeline.getBindPoint(), clustersLightCalcOffsetPipeline.getPipelineHandle());
        cmd.pushConstants<ClustersLightCalcOffsetPC>(
            clustersLightCalcOffsetPipeline.getLayoutHandle(), ClustersLightCalcOffsetPC::sStages, 0, mResources.mClustersLightCalcOffsetPc
        );
        std::uint32_t numLights = mScene.getLightIds().size();
        if (numLights == 0) return;
        cmd.dispatch(
            SwHelper::fastDivCeil(NUM_CLUSTERS, SwRenderer::MAX_2D_WORKGROUP_THREADS), SwHelper::fastDivCeil(numLights, SwRenderer::MAX_2D_WORKGROUP_THREADS), 1
        );
    });

    // Clusters Light Prefix Sum Offset
    mScene.insertPass(SwPass::Type::LightingClustersLightPrefixSumOffset, [&](vk::CommandBuffer cmd) {
        auto& clustersLightPrefixSumOffsetPipeline = mResources.mClustersLightPrefixSumOffsetPipelineBundle;
        cmd.bindPipeline(clustersLightPrefixSumOffsetPipeline.getBindPoint(), clustersLightPrefixSumOffsetPipeline.getPipelineHandle());
        cmd.pushConstants<ClustersLightPrefixSumOffsetPC>(
            clustersLightPrefixSumOffsetPipeline.getLayoutHandle(), ClustersLightPrefixSumOffsetPC::sStages, 0, mResources.mClustersLightPrefixSumOffsetPc
        );
        cmd.dispatch(1, 1, 1);
    });

    // Clusters Light Select
    mScene.insertPass(SwPass::Type::LightingClustersLightSelect, [&](vk::CommandBuffer cmd) {
        auto& clustersLightSelectPipeline = mResources.mClustersLightSelectPipelineBundle;
        cmd.bindPipeline(clustersLightSelectPipeline.getBindPoint(), clustersLightSelectPipeline.getPipelineHandle());
        cmd.pushConstants<ClustersLightSelectPC>(
            clustersLightSelectPipeline.getLayoutHandle(), ClustersLightSelectPC::sStages, 0, mResources.mClustersLightSelectPc
        );
        std::uint32_t numLights = mScene.getLightIds().size();
        if (numLights == 0) return;
        cmd.dispatch(
            SwHelper::fastDivCeil(NUM_CLUSTERS, SwRenderer::MAX_2D_WORKGROUP_THREADS), SwHelper::fastDivCeil(numLights, SwRenderer::MAX_2D_WORKGROUP_THREADS), 1
        );
    });

    // Shadows Cull
    mScene.insertPass(SwPass::Type::LightingShadowsCull, [&](vk::CommandBuffer cmd) {
        auto& shadowsCullPipeline = mResources.mShadowsCullPipelineBundle;
        cmd.bindPipeline(shadowsCullPipeline.getBindPoint(), shadowsCullPipeline.getPipelineHandle());
        cmd.pushConstants<ShadowsCullPC>(shadowsCullPipeline.getLayoutHandle(), ShadowsCullPC::sStages, 0, mResources.mShadowsCullPc);
        cmd.dispatch(
            SwHelper::fastDivCeil(MAX_NUM_SHADOW_VIEWS, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(mScene.getRis().size(), SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
    });

    // Shadows Draw
    mScene.insertPass(SwPass::Type::LightingShadowsDraw, [&](vk::CommandBuffer cmd) {});
}

void SwLighting::System::reInitializeOnResize() {
    mResources.mClustersMarkActiveDescriptorSet.writeImage(
        0, SwRenderer::sRendererContext.mSwapchain->getDepthImage().getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mClustersMarkActiveDescriptorSet.pushWrites();
}

void SwLighting::System::refreshDataUsage() {
    // Reset
    {
        mResources.mResetPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer;
        mResources.mResetPc.mShadowsRcsLimit = mScene.getRcs().size() * MAX_NUM_SHADOW_VIEWS;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&mResources.mLightsVisibleIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsViewsBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowMapSlotsCount, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveIndicesBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersActiveBooleansBuffer, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersLightCounts, SwDependency::BufferDepType::TransferWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersLightWriteCursorsBuffer, SwDependency::BufferDepType::TransferWrite);
    }

    // Clusters Build
    {
        mResources.mClustersBuildPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
        mResources.mClustersBuildPc.mClustersBuffer = mResources.mClustersBuffer;
        mResources.mClustersBuildPc.mInvProj = glm::inverse(mScene.getCamera().getPerspective().getProjVk());
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
        mResources.mLightsCullPc.mLightsBuffer = mScene.getLightsBuffer();
        mResources.mLightsCullPc.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer();
        mResources.mLightsCullPc.mInstancesBuffer = mScene.getInstancesBuffer();
        mResources.mLightsCullPc.mLightsVisibleIndicesBuffer = mResources.mLightsVisibleIndicesBuffer;
        mResources.mLightsCullPc.mShadowsViewsBuffer = mResources.mShadowsViewsBuffer;
        mResources.mLightsCullPc.mShadowMapSlotsCount = mResources.mShadowMapSlotsCount;
        mResources.mLightsCullPc.mLightsCount = mScene.getLightIds().size();

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingLightsCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mLightsVisibleIndicesBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mLightsVisibleIndicesBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mReadBuffers.emplace_back(&mResources.mShadowMapSlotsCount, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowMapSlotsCount, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsViewsBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Clusters Light Calc Offset
    {
        mResources.mClustersLightCalcOffsetPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
        mResources.mClustersLightCalcOffsetPc.mLightsBuffer = mScene.getLightsBuffer();
        mResources.mClustersLightCalcOffsetPc.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer();
        mResources.mClustersLightCalcOffsetPc.mInstancesBuffer = mScene.getInstancesBuffer();
        mResources.mClustersLightCalcOffsetPc.mClustersBuffer = mResources.mClustersBuffer;
        mResources.mClustersLightCalcOffsetPc.mClustersActiveIndicesBuffer = mResources.mClustersActiveIndicesBuffer;
        mResources.mClustersLightCalcOffsetPc.mLightsVisibleIndicesBuffer = mResources.mLightsVisibleIndicesBuffer;
        mResources.mClustersLightCalcOffsetPc.mClustersLightCounts = mResources.mClustersLightCounts;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersLightCalcOffset].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mClustersBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mClustersActiveIndicesBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mLightsVisibleIndicesBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mClustersLightCounts, SwDependency::BufferDepType::ComputeStorageReadWrite);
    }

    // Clusters Light Prefix Sum Offset
    {
        mResources.mClustersLightPrefixSumOffsetPc.mClustersLightCounts = mResources.mClustersLightCounts;
        mResources.mClustersLightPrefixSumOffsetPc.mClustersLightOffsetsBuffer = mResources.mClustersLightOffsetsBuffer;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersLightPrefixSumOffset].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mResources.mClustersLightCounts, SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mClustersLightOffsetsBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Clusters Light Select
    {
        mResources.mClustersLightSelectPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();
        mResources.mClustersLightSelectPc.mLightsBuffer = mScene.getLightsBuffer();
        mResources.mClustersLightSelectPc.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer();
        mResources.mClustersLightSelectPc.mInstancesBuffer = mScene.getInstancesBuffer();
        mResources.mClustersLightSelectPc.mClustersBuffer = mResources.mClustersBuffer;
        mResources.mClustersLightSelectPc.mClustersActiveIndicesBuffer = mResources.mClustersActiveIndicesBuffer;
        mResources.mClustersLightSelectPc.mLightsVisibleIndicesBuffer = mResources.mLightsVisibleIndicesBuffer;
        mResources.mClustersLightSelectPc.mClustersLightIndicesBuffer = mResources.mClustersLightIndicesBuffer;
        mResources.mClustersLightSelectPc.mClustersLightCounts = mResources.mClustersLightCounts;
        mResources.mClustersLightSelectPc.mClustersLightOffsetsBuffer = mResources.mClustersLightOffsetsBuffer;
        mResources.mClustersLightSelectPc.mClustersLightWriteCursorsBuffer = mResources.mClustersLightWriteCursorsBuffer;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersLightSelect].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mClustersBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mClustersActiveIndicesBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mLightsVisibleIndicesBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mResources.mClustersLightOffsetsBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mClustersLightWriteCursorsBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mClustersLightIndicesBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Shadows Cull
    {
        mResources.mShadowsCullPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer;
        mResources.mShadowsCullPc.mRisBuffer = mScene.getRisBuffer();
        mResources.mShadowsCullPc.mShadowsRisIndicesBuffer = mResources.mShadowsRisIndicesBuffer;
        mResources.mShadowsCullPc.mShadowsViewsBuffer = mResources.mShadowsViewsBuffer;
        mResources.mShadowsCullPc.mLightsBuffer = mScene.getLightsBuffer();
        mResources.mShadowsCullPc.mBoundsBuffer = mScene.getBoundsBuffer();
        mResources.mShadowsCullPc.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer();
        mResources.mShadowsCullPc.mInstancesBuffer = mScene.getInstancesBuffer();
        mResources.mShadowsCullPc.mNumRcsPerShadowView = mScene.getRcs().size();
        mResources.mShadowsCullPc.mNumRisPerShadowView = mScene.getRis().size();
        mResources.mShadowsCullPc.mShadowsRisLimit = mScene.getRis().size() * MAX_NUM_SHADOW_VIEWS;

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mReadBuffers.emplace_back(&mScene.getRisBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
        d.mReadBuffers.emplace_back(&mResources.mShadowsViewsBuffer, SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getLightsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    }

    // Shadows Draw
    {
        mResources.mShadowsDrawPc.mShadowsRcsBuffer = mResources.mShadowsRcsBuffer;
        mResources.mShadowsDrawPc.mShadowsRisIndicesBuffer = mResources.mShadowsRisIndicesBuffer;
        mResources.mShadowsDrawPc.mLightsBuffer = mScene.getLightsBuffer();
        mResources.mShadowsDrawPc.mVertexBuffer = mScene.getVertexBuffer();
        mResources.mShadowsDrawPc.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer();
        mResources.mShadowsDrawPc.mInstancesBuffer = mScene.getInstancesBuffer();
        mResources.mShadowsDrawPc.mMaterialConstantsBuffer = mScene.getMaterialConstantsBuffer();

        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsDraw].getDeps();
        d.clear();
        for (std::size_t i = 0; i < MAX_DIRECTIONAL_SHADOW_MAPS; i++)
            d.mWriteImages.emplace_back(&mResources.mDirectionalShadowMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
        for (std::size_t i = 0; i < MAX_SPOT_SHADOW_MAPS; i++)
            d.mWriteImages.emplace_back(&mResources.mSpotShadowMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
        d.mReadBuffers.emplace_back(&mResources.mShadowsRcsBuffer, SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&mResources.mShadowsRisIndicesBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    }
}