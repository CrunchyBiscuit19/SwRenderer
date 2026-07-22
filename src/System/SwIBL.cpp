#include <Renderer/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwStagingRing.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwIBL.h>
#include <stb_image.h>
#include <tinyexr.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

SwDescriptorLayout SwIBL::Resources::sConsumeDescriptorLayout{};

void SwIBL::Resources::init() {
    SwIBL::Resources::sConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLConsumeDescriptorSetLayout",
        {
            {CONSUME_IRRADIANCE_SAMPLER_BINDING, vk::DescriptorType::eSampler, 1},
            {CONSUME_PREFILTER_SAMPLER_BINDING, vk::DescriptorType::eSampler, 1},
            {CONSUME_BRDF_LUT_SAMPLER_BINDING, vk::DescriptorType::eSampler, 1},
            {CONSUME_IRRADIANCE_IMAGE_BINDING, vk::DescriptorType::eSampledImage, 1},
            {CONSUME_PREFILTER_IMAGE_BINDING, vk::DescriptorType::eSampledImage, 1},
            {CONSUME_BRDF_LUT_IMAGE_BINDING, vk::DescriptorType::eSampledImage, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwIBL::Resources::cleanup() { SwIBL::Resources::sConsumeDescriptorLayout.destroy(); }

SwIBL::System::System(SwScene& scene) : SwSystem(scene) {}

void SwIBL::System::initializeResources() {
    SwIBL::Resources::sConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLConsumeDescriptorSetLayout",
        {
            {CONSUME_IRRADIANCE_SAMPLER_BINDING, vk::DescriptorType::eSampler, 1},
            {CONSUME_PREFILTER_SAMPLER_BINDING, vk::DescriptorType::eSampler, 1},
            {CONSUME_BRDF_LUT_SAMPLER_BINDING, vk::DescriptorType::eSampler, 1},
            {CONSUME_IRRADIANCE_IMAGE_BINDING, vk::DescriptorType::eSampledImage, 1},
            {CONSUME_PREFILTER_IMAGE_BINDING, vk::DescriptorType::eSampledImage, 1},
            {CONSUME_BRDF_LUT_IMAGE_BINDING, vk::DescriptorType::eSampledImage, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );

    vk::SamplerCreateInfo envSamplerInfo{};
    envSamplerInfo.setMagFilter(vk::Filter::eLinear);
    envSamplerInfo.setMinFilter(vk::Filter::eLinear);
    envSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    envSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);       // longitude wraps
    envSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);  // latitude clamps at the poles
    envSamplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    envSamplerInfo.setMinLod(0.f);
    envSamplerInfo.setMaxLod(VK_LOD_CLAMP_NONE);  // allow trilinear sampling across the full prefilter chain
    mResources.mEnvSampler = SwSamplerFactory::createSampler("IBLEnvSampler", envSamplerInfo);

    vk::SamplerCreateInfo lutSamplerInfo{};
    lutSamplerInfo.setMagFilter(vk::Filter::eLinear);
    lutSamplerInfo.setMinFilter(vk::Filter::eLinear);
    lutSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
    lutSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    lutSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    lutSamplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    mResources.mLutSampler = SwSamplerFactory::createSampler("IBLLutSampler", lutSamplerInfo);

    mResources.mConsumeDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLConsumeDescriptorSet", SwIBL::Resources::sConsumeDescriptorLayout);

    const vk::ImageUsageFlags iblUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    mResources.mIrradianceImage = SwImageFactory::createColorImage2D("IBLIrradianceImage", FORMAT, IRRADIANCE_EXTENT, iblUsage, false);
    mResources.mPrefilterImage = SwImageFactory::createColorImage2D("IBLPrefilterImage", FORMAT, PREFILTER_EXTENT, iblUsage, true);
    mResources.mBrdfLutImage = SwImageFactory::createColorImage2D("IBLBrdfLutImage", BRDF_LUT_FORMAT, BRDF_LUT_EXTENT, iblUsage, false);

    mPrefilterMipLevels = SwHelper::calculateMipMapLevels(PREFILTER_EXTENT);
    for (std::uint32_t mip = 0; mip < mPrefilterMipLevels; mip++) {
        mResources.mPrefilterImage.addImageView(
            "IBLPrefilterMip" + std::to_string(mip), FORMAT, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, mip, 1
        );
    }

    mResources.mIrradianceDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLBakeInputDescriptorSetLayout",
        {{0, vk::DescriptorType::eSampler, 1}, {1, vk::DescriptorType::eSampledImage, 1}, {2, vk::DescriptorType::eStorageImage, 1}},
        vk::ShaderStageFlagBits::eCompute
    );
    mResources.mPrefilterDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLPrefilterDescriptorSetLayout",
        {{0, vk::DescriptorType::eSampler, 1}, {1, vk::DescriptorType::eSampledImage, 1}, {2, vk::DescriptorType::eStorageImage, MAX_PREFILTER_MIP_LEVELS}},
        vk::ShaderStageFlagBits::eCompute
    );
    mResources.mBrdfLutDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLBrdfLutDescriptorSetLayout", {{0, vk::DescriptorType::eStorageImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );

    mResources.mIrradiancePipelineLayout =
        SwPipelineFactory::createPipelineLayout("IBLIrradiancePipelineLayout", mResources.mIrradianceDescriptorLayout.getHandle(), {});
    mResources.mPrefilterPipelineLayout = SwPipelineFactory::createPipelineLayout(
        "IBLPrefilterPipelineLayout", mResources.mPrefilterDescriptorLayout.getHandle(), SwIBL::PrefilterPC::getRange()
    );
    mResources.mBrdfLutPipelineLayout =
        SwPipelineFactory::createPipelineLayout("IBLBrdfLutPipelineLayout", mResources.mBrdfLutDescriptorLayout.getHandle(), {});

    SwShader irradianceShader = SwShaderFactory::createShader("IBLIrradianceShaderModule", IRRADIANCE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    SwShader prefilterShader = SwShaderFactory::createShader("IBLPrefilterShaderModule", PREFILTER_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    SwShader brdfLutShader = SwShaderFactory::createShader("IBLBrdfLutShaderModule", BRDF_LUT_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);

    mResources.mIrradiancePipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "IBLIrradiancePipeline", {irradianceShader.getHandle(), mResources.mIrradiancePipelineLayout.getHandle()}
    );
    mResources.mPrefilterPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("IBLPrefilterPipeline", {prefilterShader.getHandle(), mResources.mPrefilterPipelineLayout.getHandle()});
    mResources.mBrdfLutPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("IBLBrdfLutPipeline", {brdfLutShader.getHandle(), mResources.mBrdfLutPipelineLayout.getHandle()});

    mResources.mIrradianceDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLIrradianceDescriptorSet", mResources.mIrradianceDescriptorLayout);
    mResources.mIrradianceDescriptorSet.writeImage(2, mResources.mIrradianceImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eGeneral);
    mResources.mIrradianceDescriptorSet.pushWrites();

    mResources.mPrefilterMipDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLPrefilterDescriptorSet", mResources.mPrefilterDescriptorLayout);
    for (std::uint32_t slot = 0; slot < MAX_PREFILTER_MIP_LEVELS; slot++) {
        const std::uint32_t mip = std::min(slot, mPrefilterMipLevels - 1);
        mResources.mPrefilterMipDescriptorSet.writeImage(2, mResources.mPrefilterImage.getOtherImageViewHandle(mip), nullptr, vk::ImageLayout::eGeneral, slot);
    }
    mResources.mPrefilterMipDescriptorSet.pushWrites();

    mResources.mBrdfLutDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLBrdfLutDescriptorSet", mResources.mBrdfLutDescriptorLayout);
    mResources.mBrdfLutDescriptorSet.writeImage(0, mResources.mBrdfLutImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eGeneral);
    mResources.mBrdfLutDescriptorSet.pushWrites();

    // Bake the environment-independent BRDF LUT once
    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&](vk::CommandBuffer cmd) {
        mResources.mIrradianceImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);
        mResources.mPrefilterImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::ImageLayout::eGeneral);

        mResources.mBrdfLutImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral);
        cmd.bindPipeline(mResources.mBrdfLutPipelineBundle.getBindPoint(), mResources.mBrdfLutPipelineBundle.getPipelineHandle());
        cmd.bindDescriptorSets(
            mResources.mBrdfLutPipelineBundle.getBindPoint(),
            mResources.mBrdfLutPipelineBundle.getLayoutHandle(),
            0,
            mResources.mBrdfLutDescriptorSet.getHandle(),
            nullptr
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(BRDF_LUT_EXTENT.width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(BRDF_LUT_EXTENT.height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
        mResources.mBrdfLutImage.emitTransition(
            cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal
        );
        mResources.mIrradianceImage.emitTransition(
            cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal
        );
        mResources.mPrefilterImage.emitTransition(
            cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal
        );
    });

    mResources.mConsumeDescriptorSet.writeSampler(CONSUME_IRRADIANCE_SAMPLER_BINDING, mResources.mEnvSampler.getHandle());
    mResources.mConsumeDescriptorSet.writeSampler(CONSUME_PREFILTER_SAMPLER_BINDING, mResources.mEnvSampler.getHandle());
    mResources.mConsumeDescriptorSet.writeSampler(CONSUME_BRDF_LUT_SAMPLER_BINDING, mResources.mLutSampler.getHandle());
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_IRRADIANCE_IMAGE_BINDING, mResources.mIrradianceImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_PREFILTER_IMAGE_BINDING, mResources.mPrefilterImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_BRDF_LUT_IMAGE_BINDING, mResources.mBrdfLutImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.pushWrites();

    // Skybox
    mResources.mSkyboxSampler = SwSamplerFactory::createSampler("SkyboxDrawSampler", vk::SamplerCreateInfo());

    mResources.mSkyboxDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "SkyboxDrawDescriptorSetLayout",
        {{0, vk::DescriptorType::eSampler, 1}, {1, vk::DescriptorType::eSampledImage, 1}},
        vk::ShaderStageFlagBits::eFragment
    );
    mResources.mSkyboxDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("SkyboxDrawDescriptorSet", mResources.mSkyboxDescriptorLayout);

    mResources.mSkyboxPipelineLayout =
        SwPipelineFactory::createPipelineLayout("SkyboxDrawPipelineLayout", mResources.mSkyboxDescriptorLayout.getHandle(), SwIBL::SkyboxPC::getRange());

    SwShader skyboxVertexShader = SwShaderFactory::createShader("SkyboxVertexShaderModule", SKYBOX_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader skyboxFragmentShader =
        SwShaderFactory::createShader("SkyboxFragmentShaderModule", SKYBOX_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

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
    skyboxPipelineOptions.mVertexShader = skyboxVertexShader.getHandle();
    skyboxPipelineOptions.mFragmentShader = skyboxFragmentShader.getHandle();
    skyboxPipelineOptions.mLayout = mResources.mSkyboxPipelineLayout.getHandle();
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
    mResources.mSkyboxPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("SkyboxDrawPipeline", skyboxPipelineOptions);

    const std::uint64_t skyboxVertexSize = mResources.mSkyboxVertices.size() * sizeof(float);
    mResources.mSkyboxVertexBuffer =
        SwBufferFactory::createAllocatedBuffer("SkyboxDrawVertexBuffer", vk::BufferUsageFlagBits::eStorageBuffer, 0, skyboxVertexSize, true);

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this, skyboxVertexSize](vk::CommandBuffer cmd) {
        SwRenderer::sRendererContext.mStagingRing->upload(cmd, mResources.mSkyboxVertexBuffer, mResources.mSkyboxVertices.data(), skyboxVertexSize);
    });

    reinitializeOnUpdate(SKYBOX_DEFAULT_HDR_PATH);
}

void SwIBL::System::initializePasses() {
    mScene.insertPass(SwPass::Type::IBLSkybox, [&](vk::CommandBuffer cmd) {
        const vk::RenderingAttachmentInfo colorAttachment = SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
        const vk::RenderingAttachmentInfo depthAttachment = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo =
            SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mResources.mSkyboxPipelineBundle.getBindPoint(), mResources.mSkyboxPipelineBundle.getPipelineHandle());
        cmd.bindDescriptorSets(
            mResources.mSkyboxPipelineBundle.getBindPoint(),
            mResources.mSkyboxPipelineBundle.getLayoutHandle(),
            0,
            mResources.mSkyboxDescriptorSet.getHandle(),
            nullptr
        );
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());
        cmd.pushConstants<SwIBL::SkyboxPC>(mResources.mSkyboxPipelineBundle.getLayoutHandle(), SwIBL::SkyboxPC::sStages, 0, mResources.mSkyboxPushConstants);
        cmd.draw(SwIBL::NUM_SKYBOX_VERTICES, 1, 0, 0);

        cmd.endRendering();
    });
}

void SwIBL::System::refreshDependencies() {
    // Skybox
    SwDependency& d = mScene.mPasses[SwPass::Type::IBLSkybox].getDeps();
    d.clear();
    d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    d.mReadImages.emplace_back(&mResources.mSkyboxImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    d.mReadBuffers.emplace_back(&mResources.mSkyboxVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);
}

void SwIBL::System::bakeFromEnvironment(SwImage& environment, vk::Sampler environmentSampler) {
    vk::ImageView environmentView = environment.getMainImageViewHandle();

    // Bind the freshly-loaded environment as the input (sampler at binding 0, image at binding 1) of every bake set.
    mResources.mIrradianceDescriptorSet.writeSampler(0, environmentSampler);
    mResources.mIrradianceDescriptorSet.writeImage(1, environmentView, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mIrradianceDescriptorSet.pushWrites();
    mResources.mPrefilterMipDescriptorSet.writeSampler(0, environmentSampler);
    mResources.mPrefilterMipDescriptorSet.writeImage(1, environmentView, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mPrefilterMipDescriptorSet.pushWrites();

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [&](vk::CommandBuffer cmd) {
        environment.emitTransition(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);
        mResources.mIrradianceImage.emitTransition(
            cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral
        );
        mResources.mPrefilterImage.emitTransition(
            cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral
        );

        // Diffuse irradiance.
        cmd.bindPipeline(mResources.mIrradiancePipelineBundle.getBindPoint(), mResources.mIrradiancePipelineBundle.getPipelineHandle());
        cmd.bindDescriptorSets(
            mResources.mIrradiancePipelineBundle.getBindPoint(),
            mResources.mIrradiancePipelineBundle.getLayoutHandle(),
            0,
            mResources.mIrradianceDescriptorSet.getHandle(),
            nullptr
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(IRRADIANCE_EXTENT.width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(IRRADIANCE_EXTENT.height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );

        // Specular prefilter: roughness rising with mip level.
        cmd.bindPipeline(mResources.mPrefilterPipelineBundle.getBindPoint(), mResources.mPrefilterPipelineBundle.getPipelineHandle());
        cmd.bindDescriptorSets(
            mResources.mPrefilterPipelineBundle.getBindPoint(),
            mResources.mPrefilterPipelineBundle.getLayoutHandle(),
            0,
            mResources.mPrefilterMipDescriptorSet.getHandle(),
            nullptr
        );
        mResources.mPrefilterPushConstants.mTotalMipLevels = mPrefilterMipLevels;
        mResources.mPrefilterPushConstants.mBaseExtent = {PREFILTER_EXTENT.width, PREFILTER_EXTENT.height};
        cmd.pushConstants<SwIBL::PrefilterPC>(
            mResources.mPrefilterPipelineBundle.getLayoutHandle(), SwIBL::PrefilterPC::sStages, 0, mResources.mPrefilterPushConstants
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(PREFILTER_EXTENT.width, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(PREFILTER_EXTENT.height, SwRenderer::MAX_3D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(mPrefilterMipLevels, SwRenderer::MAX_3D_WORKGROUP_THREADS)
        );
    });

    // The image views are unchanged, but re-point the consume set so the equirect sampler stays attached.
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_IRRADIANCE_IMAGE_BINDING, mResources.mIrradianceImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_PREFILTER_IMAGE_BINDING, mResources.mPrefilterImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.pushWrites();
}

void SwIBL::System::reinitializeOnUpdate(std::optional<std::filesystem::path> newLoadFile) {
    mLoadFromFile = newLoadFile;
    if (!mLoadFromFile.has_value()) {
        return;
    }

    const std::string pathString = mLoadFromFile.value().string();
    std::int32_t width = 0;
    std::int32_t height = 0;

    std::string extension = mLoadFromFile.value().extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool isExr = extension == ".exr";
    float* data = nullptr;

    if (isExr) {
        const char* exrError = nullptr;
        if (LoadEXR(&data, &width, &height, pathString.c_str(), &exrError) != TINYEXR_SUCCESS) {
            if (exrError) {
                FreeEXRErrorMessage(exrError);
            }
            return;
        }
        const std::size_t rowFloats = static_cast<std::size_t>(width) * 4;
        std::vector<float> rowScratch(rowFloats);
        for (std::int32_t y = 0; y < height / 2; y++) {
            float* topRow = data + static_cast<std::size_t>(y) * rowFloats;
            float* bottomRow = data + static_cast<std::size_t>(height - 1 - y) * rowFloats;
            std::memcpy(rowScratch.data(), topRow, rowFloats * sizeof(float));
            std::memcpy(topRow, bottomRow, rowFloats * sizeof(float));
            std::memcpy(bottomRow, rowScratch.data(), rowFloats * sizeof(float));
        }
    } else {
        std::int32_t numChannels = 0;
        stbi_set_flip_vertically_on_load(true);
        data = stbi_loadf(pathString.c_str(), &width, &height, &numChannels, 4);
    }

    if (!data || width == 0 || height == 0) {
        return;
    }

    // Cosine-weight each equirect row by its latitude (rows near the poles cover less solid angle) and average
    // the Rec.709 luminance. Used to normalize the IBL ambient so its strength is decoupled from the HDR's scale.

    double weightedLuminance = 0.0;
    double totalWeight = 0.0;
    for (std::int32_t y = 0; y < height; y++) {
        const float latitude = (((static_cast<float>(y) + 0.5f) / static_cast<float>(height)) - 0.5f) * glm::pi<float>();
        const double rowWeight = std::cos(latitude);
        for (std::int32_t x = 0; x < width; x++) {
            const float* texel = data + (static_cast<std::size_t>(y) * width + x) * 4;
            const double luminance = 0.2126 * texel[0] + 0.7152 * texel[1] + 0.0722 * texel[2];
            weightedLuminance += luminance * rowWeight;
            totalWeight += rowWeight;
        }
    }
    const double average = totalWeight > 0.0 ? weightedLuminance / totalWeight : 1.0;
    mEnvAvgLuminance = std::max(static_cast<float>(average), 1e-4f);

    mResources.mSkyboxImage = SwImageFactory::createColorImage2D(
        "IBLSkyboxImage",
        vk::Format::eR32G32B32A32Sfloat,
        vk::Extent3D{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        true
    );
    mResources.mSkyboxImage.fillImageData(data);

    if (isExr) {
        std::free(data);  // tinyexr allocates the RGBA buffer with malloc
    } else {
        stbi_image_free(data);
    }

    mResources.mSkyboxDescriptorSet.writeSampler(0, mResources.mSkyboxSampler.getHandle());
    mResources.mSkyboxDescriptorSet.writeImage(1, mResources.mSkyboxImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mSkyboxDescriptorSet.pushWrites();

    // Bake with the equirect sampler (full LOD range) so the prefilter can read the environment's mip
    // chain for PDF-based mip selection; mDrawSampler clamps maxLod to 0 and would defeat that.
    bakeFromEnvironment(mResources.mSkyboxImage, mResources.mEnvSampler.getHandle());
}

void SwIBL::System::refreshPushConstants() {
    mResources.mSkyboxPushConstants.mSkyboxVertexBuffer = mResources.mSkyboxVertexBuffer.getDeviceAddress().value();
    mResources.mSkyboxPushConstants.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer().getDeviceAddress().value();
}
