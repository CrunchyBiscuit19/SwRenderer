#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwFXAA.h>
#include <Scene/SwScene.h>
#include <quill/LogMacros.h>

SwFXAA::System::System(SwScene& scene) : SwSystem(scene) {}

void SwFXAA::System::initializeResources() {
    // Binding 0: the draw image sampled as the FXAA input.
    // Binding 1: the bilinear sampler used to fetch neighbouring luma.
    // Binding 2: the same draw image bound as a storage image so the resolve is written back in place.
    mResources.mWorkDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampler, 1}, {2, vk::DescriptorType::eStorageImage, 1}},
        vk::ShaderStageFlagBits::eCompute
    );
    mResources.mWorkDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mWorkDescriptorLayout);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear);
    samplerInfo.setMinFilter(vk::Filter::eLinear);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
    samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    mResources.mWorkSampler = SwSamplerFactory::createSampler(samplerInfo);
    mResources.mWorkDescriptorSet.writeSampler(1, mResources.mWorkSampler.getRawSampler());

    mResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout(mResources.mWorkDescriptorLayout.getRawLayout(), SwFXAA::WorkPC::getRange());
    SwShader workShader = SwShaderFactory::createShader(FXAA_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mWorkPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mResources.mWorkPipelineLayout.getRawLayout()});

    reInitializeOnResize();
}

void SwFXAA::System::initializePasses() {
    SwDependency staticDeps;

    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ComputeStorageReadWrite);
    mScene.insertPass(SwPass::Type::FXAAWork, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());

        cmd.bindDescriptorSets(
            mResources.mWorkPipelineBundle.getBindPoint(),
            mResources.mWorkPipelineBundle.getRawLayout(),
            0,
            mResources.mWorkDescriptorSet.getRawSet(),
            nullptr
        );

        cmd.pushConstants<SwFXAA::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwFXAA::WorkPC::sStages, 0, mResources.mWorkPushConstants);

        vk::Extent3D drawExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
        cmd.dispatch(
            SwHelper::fastDivCeil(drawExtent.width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(drawExtent.height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
    });
    staticDeps.clear();
}

void SwFXAA::System::reInitializeOnResize() {
    // The draw image is recreated on resize, so rebind both views and refresh the texel size the shader samples with.
    vk::ImageView drawImageView = SwRenderer::sRendererContext.mSwapchain->getDrawImage().getRawMainImageView();
    mResources.mWorkDescriptorSet.writeImage(0, drawImageView, nullptr, vk::ImageLayout::eGeneral);
    mResources.mWorkDescriptorSet.writeImage(2, drawImageView, nullptr, vk::ImageLayout::eGeneral);
    mResources.mWorkDescriptorSet.pushWrites();

    vk::Extent2D drawExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D();
    mResources.mWorkPushConstants.mInverseScreenSize = glm::vec2(1.f / drawExtent.width, 1.f / drawExtent.height);
}
