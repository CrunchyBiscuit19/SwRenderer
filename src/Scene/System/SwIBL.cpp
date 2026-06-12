#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwIBL.h>
#include <Scene/SwScene.h>

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
}

void SwIBL::System::initializePasses() {}

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

        mResources.mIrradianceImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);
        mResources.mPrefilterImage.emitTransition(cmd, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);
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
