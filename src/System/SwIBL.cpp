#include <Renderer/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <System/SwIBL.h>
#include <Scene/SwScene.h>
#include <stb_image.h>

SwDescriptorLayout SwIBL::System::sConsumeDescriptorLayout{};

void SwIBL::System::init() {
    sConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLConsumeDescriptorSetLayout",
        {
            {CONSUME_IRRADIANCE_BINDING, vk::DescriptorType::eCombinedImageSampler, 1},
            {CONSUME_PREFILTER_BINDING, vk::DescriptorType::eCombinedImageSampler, 1},
            {CONSUME_BRDF_LUT_BINDING, vk::DescriptorType::eCombinedImageSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwIBL::System::cleanup() { sConsumeDescriptorLayout.destroy(); }

SwIBL::System::System(SwScene& scene) : SwSystem(scene) {}

void SwIBL::System::initializeResources() {
    sConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLConsumeDescriptorSetLayout",
        {
            {CONSUME_IRRADIANCE_BINDING, vk::DescriptorType::eCombinedImageSampler, 1},
            {CONSUME_PREFILTER_BINDING, vk::DescriptorType::eCombinedImageSampler, 1},
            {CONSUME_BRDF_LUT_BINDING, vk::DescriptorType::eCombinedImageSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );

    // --- Samplers ---
    vk::SamplerCreateInfo envSamplerInfo{};
    envSamplerInfo.setMagFilter(vk::Filter::eLinear);
    envSamplerInfo.setMinFilter(vk::Filter::eLinear);
    envSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    envSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);        // longitude wraps
    envSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);   // latitude clamps at the poles
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

    // --- The set-1 descriptor set the geometry shaders bind ---
    mResources.mConsumeDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLConsumeDescriptorSet", sConsumeDescriptorLayout);

    // --- Baked maps (storage for the compute bakes, sampled by the geometry shaders) ---
    const vk::ImageUsageFlags iblUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    mResources.mIrradianceImage = SwImageFactory::createColorImage2D("IBLIrradiance", nullptr, IBL_FORMAT, IRRADIANCE_EXTENT, iblUsage, false);
    mResources.mPrefilterImage = SwImageFactory::createColorImage2D("IBLPrefilter", nullptr, IBL_FORMAT, PREFILTER_EXTENT, iblUsage, true);
    mResources.mBrdfLutImage = SwImageFactory::createColorImage2D("IBLBrdfLut", nullptr, BRDF_LUT_FORMAT, BRDF_LUT_EXTENT, iblUsage, false);

    mPrefilterMipLevels = SwHelper::calculateMipMapLevels(PREFILTER_EXTENT);
    // One single-mip storage view per prefilter mip level (the main view spans all mips and is used for sampling).
    for (std::uint32_t mip = 0; mip < mPrefilterMipLevels; mip++) {
        mResources.mPrefilterImage.addImageView(
            "IBLPrefilterMip" + std::to_string(mip), IBL_FORMAT, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, mip, 1
        );
    }

    // --- Bake pipelines ---
    mResources.mBakeInputDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLBakeInputDescriptorSetLayout",
        {{0, vk::DescriptorType::eCombinedImageSampler, 1}, {1, vk::DescriptorType::eStorageImage, 1}},
        vk::ShaderStageFlagBits::eCompute
    );
    mResources.mBrdfLutDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "IBLBrdfLutDescriptorSetLayout", {{0, vk::DescriptorType::eStorageImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );

    mResources.mIrradiancePipelineLayout = SwPipelineFactory::createPipelineLayout("IBLIrradiancePipelineLayout", mResources.mBakeInputDescriptorLayout.getRawLayout(), {});
    mResources.mPrefilterPipelineLayout =
        SwPipelineFactory::createPipelineLayout("IBLPrefilterPipelineLayout", mResources.mBakeInputDescriptorLayout.getRawLayout(), SwIBL::PrefilterPC::getRange());
    mResources.mBrdfLutPipelineLayout = SwPipelineFactory::createPipelineLayout("IBLBrdfLutPipelineLayout", mResources.mBrdfLutDescriptorLayout.getRawLayout(), {});

    SwShader irradianceShader = SwShaderFactory::createShader("IBLIrradianceShaderModule", IRRADIANCE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    SwShader prefilterShader = SwShaderFactory::createShader("IBLPrefilterShaderModule", PREFILTER_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    SwShader brdfLutShader = SwShaderFactory::createShader("IBLBrdfLutShaderModule", BRDF_LUT_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);

    mResources.mIrradiancePipelineBundle =
        SwComputePipelineFactory::createComputePipeline("IBLIrradiancePipeline", {irradianceShader.getRawModule(), mResources.mIrradiancePipelineLayout.getRawLayout()});
    mResources.mPrefilterPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("IBLPrefilterPipeline", {prefilterShader.getRawModule(), mResources.mPrefilterPipelineLayout.getRawLayout()});
    mResources.mBrdfLutPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("IBLBrdfLutPipeline", {brdfLutShader.getRawModule(), mResources.mBrdfLutPipelineLayout.getRawLayout()});

    // --- Bake descriptor sets: storage outputs bound once here, environment input bound per bake ---
    mResources.mIrradianceDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLIrradianceDescriptorSet", mResources.mBakeInputDescriptorLayout);
    mResources.mIrradianceDescriptorSet.writeImage(1, mResources.mIrradianceImage.getRawMainImageView(), nullptr, vk::ImageLayout::eGeneral);
    mResources.mIrradianceDescriptorSet.pushWrites();

    mResources.mPrefilterMipDescriptorSets.reserve(mPrefilterMipLevels);
    for (std::uint32_t mip = 0; mip < mPrefilterMipLevels; mip++) {
        SwDescriptorSet mipSet =
            SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLPrefilterDescriptorSet" + std::to_string(mip), mResources.mBakeInputDescriptorLayout);
        mipSet.writeImage(1, mResources.mPrefilterImage.getRawOtherImageView(mip), nullptr, vk::ImageLayout::eGeneral);
        mipSet.pushWrites();
        mResources.mPrefilterMipDescriptorSets.emplace_back(std::move(mipSet));
    }

    mResources.mBrdfLutDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("IBLBrdfLutDescriptorSet", mResources.mBrdfLutDescriptorLayout);
    mResources.mBrdfLutDescriptorSet.writeImage(0, mResources.mBrdfLutImage.getRawMainImageView(), nullptr, vk::ImageLayout::eGeneral);
    mResources.mBrdfLutDescriptorSet.pushWrites();

    // --- Bake the environment-independent BRDF LUT once, and prime the env-dependent maps to a valid
    // sampled layout so the consume set is complete even before the first skybox bake. ---
    SwRenderer::sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        mResources.mBrdfLutImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral);
        cmd.bindPipeline(mResources.mBrdfLutPipelineBundle.getBindPoint(), mResources.mBrdfLutPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mResources.mBrdfLutPipelineBundle.getBindPoint(), mResources.mBrdfLutPipelineBundle.getRawLayout(), 0, mResources.mBrdfLutDescriptorSet.getRawSet(), nullptr
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(BRDF_LUT_EXTENT.width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(BRDF_LUT_EXTENT.height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
        mResources.mBrdfLutImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);

        // Placeholder transition until the first real environment bake fills these.
        mResources.mIrradianceImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);
        mResources.mPrefilterImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);
    });

    // Consume set: irradiance + prefilter use the equirect sampler; the LUT uses the clamp sampler.
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_IRRADIANCE_BINDING, mResources.mIrradianceImage.getRawMainImageView(), mResources.mEnvSampler.getRawSampler(), vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_PREFILTER_BINDING, mResources.mPrefilterImage.getRawMainImageView(), mResources.mEnvSampler.getRawSampler(), vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_BRDF_LUT_BINDING, mResources.mBrdfLutImage.getRawMainImageView(), mResources.mLutSampler.getRawSampler(), vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.pushWrites();

    // --- Skybox draw: cube + equirect sampler that rasterizes the environment behind the geometry. ---
    mResources.mDrawSampler = SwSamplerFactory::createSampler("SkyboxDrawSampler", vk::SamplerCreateInfo());

    mResources.mDrawDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "SkyboxDrawDescriptorSetLayout", {{0, vk::DescriptorType::eCombinedImageSampler, 1}}, vk::ShaderStageFlagBits::eFragment
    );
    mResources.mDrawDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("SkyboxDrawDescriptorSet", mResources.mDrawDescriptorLayout);

    mResources.mDrawPipelineLayout =
        SwPipelineFactory::createPipelineLayout("SkyboxDrawPipelineLayout", mResources.mDrawDescriptorLayout.getRawLayout(), SwIBL::DrawPC::getRange());

    SwShader skyboxVertexShader = SwShaderFactory::createShader("SkyboxVertexShaderModule", SKYBOX_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader skyboxFragmentShader = SwShaderFactory::createShader("SkyboxFragmentShaderModule", SKYBOX_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

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
    skyboxPipelineOptions.mLayout = mResources.mDrawPipelineLayout.getRawLayout();
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
    mResources.mDrawPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("SkyboxDrawPipeline", skyboxPipelineOptions);

    const std::uint32_t skyboxVertexSize = static_cast<std::uint32_t>(mResources.mDrawVertices.size() * sizeof(float));
    mResources.mDrawVertexBuffer = SwBufferFactory::createAllocatedBuffer(
        "SkyboxDrawVertexBuffer",
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        skyboxVertexSize,
        true
    );

    SwStagingBuffer skyboxVertexStagingBuffer = SwBufferFactory::createStagingBuffer("SkyboxDrawVertexStagingBuffer", skyboxVertexSize);

    vk::BufferCopy skyboxVertexCopy{};
    skyboxVertexCopy.dstOffset = 0;
    skyboxVertexCopy.srcOffset = 0;
    skyboxVertexCopy.size = skyboxVertexSize;

    SwRenderer::sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        skyboxVertexStagingBuffer.copyFromUnchecked(mResources.mDrawVertices.data(), skyboxVertexCopy.size);
        mResources.mDrawVertexBuffer.copyFrom(cmd, skyboxVertexStagingBuffer, skyboxVertexCopy);
    });

    reinitializeOnUpdate(SKYBOX_DEFAULT_HDR_PATH);
}

void SwIBL::System::initializePasses() {
    SwDependency deps;

    deps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    deps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&mResources.mDrawImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    deps.mReadBuffers.emplace_back(&mResources.mDrawVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);

    mScene.insertPass(SwPass::Type::SkyboxDraw, std::move(deps), [&](vk::CommandBuffer cmd) {
        const vk::RenderingAttachmentInfo colorAttachment = SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
        const vk::RenderingAttachmentInfo depthAttachment = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mResources.mDrawPipelineBundle.getBindPoint(), mResources.mDrawPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mResources.mDrawPipelineBundle.getBindPoint(), mResources.mDrawPipelineBundle.getRawLayout(), 0, mResources.mDrawDescriptorSet.getRawSet(), nullptr
        );
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());
        cmd.pushConstants<SwIBL::DrawPC>(mResources.mDrawPipelineBundle.getRawLayout(), SwIBL::DrawPC::sStages, 0, mResources.mDrawPushConstants);
        cmd.draw(SwIBL::NUM_SKYBOX_VERTICES, 1, 0, 0);
        SwRenderer::sRendererContext.mStats->mNumDrawCall++;

        cmd.endRendering();
    });
    deps.clear();
}

void SwIBL::System::initializePushConstants() { mResources.mDrawPushConstants.mDrawVertexBuffer = mResources.mDrawVertexBuffer.getDeviceAddress().value(); }

void SwIBL::System::bakeFromEnvironment(vk::ImageView environmentView, vk::Sampler environmentSampler) {
    // Bind the freshly-loaded environment as the input (binding 0) of every bake set.
    mResources.mIrradianceDescriptorSet.writeImage(0, environmentView, environmentSampler, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mIrradianceDescriptorSet.pushWrites();
    for (auto& mipSet : mResources.mPrefilterMipDescriptorSets) {
        mipSet.writeImage(0, environmentView, environmentSampler, vk::ImageLayout::eShaderReadOnlyOptimal);
        mipSet.pushWrites();
    }

    SwRenderer::sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        mResources.mIrradianceImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral);
        mResources.mPrefilterImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral);

        // Diffuse irradiance.
        cmd.bindPipeline(mResources.mIrradiancePipelineBundle.getBindPoint(), mResources.mIrradiancePipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mResources.mIrradiancePipelineBundle.getBindPoint(), mResources.mIrradiancePipelineBundle.getRawLayout(), 0, mResources.mIrradianceDescriptorSet.getRawSet(),
            nullptr
        );
        cmd.dispatch(
            SwHelper::fastDivCeil(IRRADIANCE_EXTENT.width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(IRRADIANCE_EXTENT.height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );

        // Specular prefilter: one dispatch per mip, roughness rising with mip level.
        cmd.bindPipeline(mResources.mPrefilterPipelineBundle.getBindPoint(), mResources.mPrefilterPipelineBundle.getRawPipeline());
        for (std::uint32_t mip = 0; mip < mPrefilterMipLevels; mip++) {
            cmd.bindDescriptorSets(
                mResources.mPrefilterPipelineBundle.getBindPoint(), mResources.mPrefilterPipelineBundle.getRawLayout(), 0,
                mResources.mPrefilterMipDescriptorSets[mip].getRawSet(), nullptr
            );
            SwIBL::PrefilterPC pc{};
            pc.mRoughness = mPrefilterMipLevels > 1 ? static_cast<float>(mip) / static_cast<float>(mPrefilterMipLevels - 1) : 0.f;
            cmd.pushConstants<SwIBL::PrefilterPC>(mResources.mPrefilterPipelineBundle.getRawLayout(), SwIBL::PrefilterPC::sStages, 0, pc);

            const std::uint32_t mipWidth = std::max(1u, PREFILTER_EXTENT.width >> mip);
            const std::uint32_t mipHeight = std::max(1u, PREFILTER_EXTENT.height >> mip);
            cmd.dispatch(
                SwHelper::fastDivCeil(mipWidth, SwRenderer::MAX_2D_WORKGROUP_THREADS), SwHelper::fastDivCeil(mipHeight, SwRenderer::MAX_2D_WORKGROUP_THREADS), 1
            );
        }
    });

    // The image views are unchanged, but re-point the consume set so the equirect sampler stays attached.
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_IRRADIANCE_BINDING, mResources.mIrradianceImage.getRawMainImageView(), mResources.mEnvSampler.getRawSampler(), vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.writeImage(
        CONSUME_PREFILTER_BINDING, mResources.mPrefilterImage.getRawMainImageView(), mResources.mEnvSampler.getRawSampler(), vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mConsumeDescriptorSet.pushWrites();
}

void SwIBL::System::reinitializeOnUpdate(std::optional<std::filesystem::path> newLoadFile) {
    mLoadFromFile = newLoadFile;
    if (!mLoadFromFile.has_value()) {
        return;
    }

    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t numChannels = 0;
    stbi_set_flip_vertically_on_load(true);
    float* data = stbi_loadf(mLoadFromFile.value().string().c_str(), &width, &height, &numChannels, 4);
    if (!data || width == 0 || height == 0) {
        return;
    }

    mResources.mDrawImage = SwImageFactory::createColorImage2D(
        "SkyboxDrawImage",
        data,
        vk::Format::eR32G32B32A32Sfloat,
        vk::Extent3D{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        vk::ImageUsageFlagBits::eSampled,
        true
    );

    stbi_image_free(data);

    mResources.mDrawDescriptorSet.writeImage(
        0,
        mResources.mDrawImage.getRawMainImageView(),
        mResources.mDrawSampler.getRawSampler(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mDrawDescriptorSet.pushWrites();

    bakeFromEnvironment(mResources.mDrawImage.getRawMainImageView(), mResources.mDrawSampler.getRawSampler());
}

void SwIBL::System::refreshPushConstants() {
    mResources.mDrawPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
}
