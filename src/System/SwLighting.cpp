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

void SwLighting::System::initializeResources() {
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

    mResources.mClustersBuildPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersBuildPipelineLayout", nullptr, SwLighting::ClustersBuildPC::getRange());
    SwShader clustersBuildShader =
        SwShaderFactory::createShader("ClustersBuildShaderModule", SwLighting::LIGHTING_CLUSTERS_BUILD_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersBuildPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersBuildPipeline", {clustersBuildShader.getHandle(), mResources.mClustersBuildPipelineLayout.getHandle()}
    );

    mResources.mClustersMarkActivePipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersMarkActivePipelineLayout", nullptr, SwLighting::ClustersMarkActivePC::getRange());
    SwShader clustersMarkActiveShader = SwShaderFactory::createShader(
        "ClustersMarkActiveShaderModule", SwLighting::LIGHTING_CLUSTERS_MARK_ACTIVE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mClustersMarkActivePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersMarkActivePipeline", {clustersMarkActiveShader.getHandle(), mResources.mClustersMarkActivePipelineLayout.getHandle()}
    );

    mResources.mClustersCullPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ClustersCullPipelineLayout", nullptr, SwLighting::ClustersCullPC::getRange());
    SwShader clustersCullShader =
        SwShaderFactory::createShader("ClustersCullShaderModule", SwLighting::LIGHTING_CLUSTERS_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mClustersCullPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ClustersCullPipeline", {clustersCullShader.getHandle(), mResources.mClustersCullPipelineLayout.getHandle()}
    );

    mResources.mShadowsResetPipelineLayout =
        SwPipelineFactory::createPipelineLayout("ShadowsResetPipelineLayout", nullptr, SwLighting::ShadowsResetPC::getRange());
    SwShader shadowsResetShader =
        SwShaderFactory::createShader("ShadowsResetShaderModule", SwLighting::LIGHTING_SHADOWS_RESET_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowsResetPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ShadowsResetPipeline", {shadowsResetShader.getHandle(), mResources.mShadowsResetPipelineLayout.getHandle()}
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
    // Clusters Build
    mScene.insertPass(SwPass::Type::LightingClustersBuild, [&](vk::CommandBuffer cmd) {
        cmd.fillBuffer(mScene.getSceneLightsInfoBuffer().getHandle(), 0, vk::WholeSize, 0);
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
    });

    // Clusters Mark Active
    mScene.insertPass(SwPass::Type::LightingClustersMarkActive, [&](vk::CommandBuffer cmd) {
        auto& clustersMarkActivePipeline = mResources.mClustersMarkActivePipelineBundle;
        cmd.bindPipeline(clustersMarkActivePipeline.getBindPoint(), clustersMarkActivePipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ClustersMarkActivePC>(
            clustersMarkActivePipeline.getLayoutHandle(), SwLighting::ClustersMarkActivePC::sStages, 0, mResources.mClustersMarkActivePc
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(LIGHTING_CLUSTERS_DIMENSIONS.x, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(LIGHTING_CLUSTERS_DIMENSIONS.y, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(LIGHTING_CLUSTERS_DIMENSIONS.z, SwRenderer::MAX_3D_WORKGROUP_THREADS)
        );
    });

    // Clusters Cull
    mScene.insertPass(SwPass::Type::LightingClustersCull, [&](vk::CommandBuffer cmd) {});

    // Shadows Reset
    mScene.insertPass(SwPass::Type::LightingShadowsReset, [&](vk::CommandBuffer cmd) {
        cmd.fillBuffer(mScene.getSceneShadowsRcsBuffer().getHandle(), 0, vk::WholeSize, 0);
        auto& shadowsResetPipeline = mResources.mShadowsResetPipelineBundle;
        cmd.bindPipeline(shadowsResetPipeline.getBindPoint(), shadowsResetPipeline.getPipelineHandle());
        cmd.pushConstants<SwLighting::ShadowsResetPC>(
            shadowsResetPipeline.getLayoutHandle(), SwLighting::ShadowsResetPC::sStages, 0, mResources.mShadowsResetPc
        );
        cmd.dispatch(SwHelper::fastDivCeil(mResources.mShadowsResetPc.mSceneShadowsRcsLimit, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    });

    // Shadows Cull
    mScene.insertPass(SwPass::Type::LightingShadowsCull, [&](vk::CommandBuffer cmd) {});

    // Shadows Draw
    mScene.insertPass(SwPass::Type::LightingShadowsDraw, [&](vk::CommandBuffer cmd) {});
}

void SwLighting::System::refreshDependencies() {
    // Clusters Build
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersBuild].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::ComputeStorageRead
        );
        d.mWriteBuffers.emplace_back(&mScene.getSceneLightsInfoBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneClustersBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Clusters Mark Active
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingClustersBuild].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mScene.getSceneClustersBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
        d.mWriteBuffers.emplace_back(&mScene.getSceneClustersActiveIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
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

    // Shadows Reset
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsReset].getDeps();
        d.clear();
        d.mWriteBuffers.emplace_back(&mScene.getSceneShadowsRcsBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneShadowsRisIndicesBuffer(), SwDependency::BufferDepType::TransferWrite);
    }

    // Shadows Cull
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::LightingShadowsCull].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mScene.getSceneShadowsRcsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneShadowsRcsBuffer(), SwDependency::BufferDepType::ComputeStorageReadWrite);
        d.mWriteBuffers.emplace_back(&mScene.getSceneShadowsRisIndicesBuffer(), SwDependency::BufferDepType::ComputeStorageWrite);
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
        d.mReadBuffers.emplace_back(&mScene.getSceneShadowsRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneShadowsRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    }
}

void SwLighting::System::refreshPushConstants() {
    mResources.mClustersBuildPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mClustersBuildPc.mSceneClustersBuffer = SwRenderer::sRendererContext.mScene->getSceneClustersBuffer().getDeviceAddress().value();
    mResources.mClustersBuildPc.mInvProj = glm::inverse(SwRenderer::sRendererContext.mScene->getCamera().getPerspective().getProjVk());
    mResources.mClustersBuildPc.mTargetSize =
        glm::uvec2(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().width, SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().height);

    mResources.mClustersMarkActivePc.mSceneClustersBuffer = SwRenderer::sRendererContext.mScene->getSceneClustersBuffer().getDeviceAddress().value();
    mResources.mClustersMarkActivePc.mSceneClustersActiveIndicesBuffer =
        SwRenderer::sRendererContext.mScene->getSceneClustersActiveIndicesBuffer().getDeviceAddress().value();

    mResources.mClustersCullPc.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneLightsInfoBuffer = mScene.getSceneLightsInfoBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mClustersCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();

    mResources.mShadowsResetPc.mSceneShadowsRcsBuffer = SwRenderer::sRendererContext.mScene->getSceneShadowsRcsBuffer().getDeviceAddress().value();
    mResources.mShadowsResetPc.mSceneShadowsRcsLimit = SwRenderer::sRendererContext.mScene->getSceneRcs().size() * MAX_NUM_SHADOW_CASTERS;

    mResources.mShadowsCullPc.mSceneShadowsRcsBuffer = SwRenderer::sRendererContext.mScene->getSceneShadowsRcsBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneShadowsRisIndicesBuffer = SwRenderer::sRendererContext.mScene->getSceneShadowsRisIndicesBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneLightsInfoBuffer = mScene.getSceneLightsInfoBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneBoundsBuffer = SwRenderer::sRendererContext.mScene->getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowsCullPc.mSceneShadowsRisLimit = SwRenderer::sRendererContext.mScene->getSceneRis().size() * MAX_NUM_SHADOW_CASTERS;

    mResources.mShadowsDrawPc.mSceneShadowsRcsBuffer = SwRenderer::sRendererContext.mScene->getSceneShadowsRcsBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneShadowsRisIndicesBuffer = SwRenderer::sRendererContext.mScene->getSceneShadowsRisIndicesBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneLightsBuffer = SwRenderer::sRendererContext.mScene->getSceneLightsBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneLightsInfoBuffer = mScene.getSceneLightsInfoBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneVertexBuffer = SwRenderer::sRendererContext.mScene->getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowsDrawPc.mSceneMaterialConstantsBuffer = SwRenderer::sRendererContext.mScene->getSceneMaterialConstantsBuffer().getDeviceAddress().value();
}