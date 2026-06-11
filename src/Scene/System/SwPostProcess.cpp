#include <Misc/SwHelper.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwPostProcess.h>
#include <Scene/SwScene.h>
#include <quill/LogMacros.h>

SwPostProcess::System::System(SwScene& scene) : SwSystem(scene) {}

void SwPostProcess::System::initializeResources() {
    // --- Tonemap: the HDR draw image bound as a single storage image, resolved to LDR in place. ---
    mResources.mTonemapDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eStorageImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mTonemapDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mTonemapDescriptorLayout);

    mResources.mTonemapPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mTonemapDescriptorLayout.getRawLayout(), SwPostProcess::TonemapPC::getRange());
    SwShader tonemapShader = SwShaderFactory::createShader(TONEMAP_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mTonemapPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({tonemapShader.getRawModule(), mResources.mTonemapPipelineLayout.getRawLayout()});

    // --- FXAA: binding 0 the draw image sampled, binding 1 a bilinear sampler, binding 2 the same image as storage (in-place resolve). ---
    mResources.mFXAADescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampler, 1}, {2, vk::DescriptorType::eStorageImage, 1}},
        vk::ShaderStageFlagBits::eCompute
    );
    mResources.mFXAADescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mFXAADescriptorLayout);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear);
    samplerInfo.setMinFilter(vk::Filter::eLinear);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
    samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    mResources.mFXAASampler = SwSamplerFactory::createSampler(samplerInfo);
    mResources.mFXAADescriptorSet.writeSampler(1, mResources.mFXAASampler.getRawSampler());

    mResources.mFXAAPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mFXAADescriptorLayout.getRawLayout(), SwPostProcess::FXAAPC::getRange());
    SwShader fxaaShader = SwShaderFactory::createShader(FXAA_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mFXAAPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({fxaaShader.getRawModule(), mResources.mFXAAPipelineLayout.getRawLayout()});

    reInitializeOnResize();
}

void SwPostProcess::System::initializePasses() {
    SwDependency staticDeps;

    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ComputeStorageReadWrite);
    mScene.insertPass(SwPass::Type::Tonemap, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mTonemapPipelineBundle.getBindPoint(), mResources.mTonemapPipelineBundle.getRawPipeline());

        cmd.bindDescriptorSets(
            mResources.mTonemapPipelineBundle.getBindPoint(), mResources.mTonemapPipelineBundle.getRawLayout(), 0, mResources.mTonemapDescriptorSet.getRawSet(),
            nullptr
        );

        cmd.pushConstants<SwPostProcess::TonemapPC>(
            mResources.mTonemapPipelineBundle.getRawLayout(), SwPostProcess::TonemapPC::sStages, 0, mResources.mTonemapPushConstants
        );

        vk::Extent3D drawExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
        cmd.dispatch(
            SwHelper::fastDivCeil(drawExtent.width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(drawExtent.height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
    });
    staticDeps.clear();

    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ComputeStorageReadWrite);
    mScene.insertPass(SwPass::Type::FXAA, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mFXAAPipelineBundle.getBindPoint(), mResources.mFXAAPipelineBundle.getRawPipeline());

        cmd.bindDescriptorSets(
            mResources.mFXAAPipelineBundle.getBindPoint(), mResources.mFXAAPipelineBundle.getRawLayout(), 0, mResources.mFXAADescriptorSet.getRawSet(), nullptr
        );

        cmd.pushConstants<SwPostProcess::FXAAPC>(
            mResources.mFXAAPipelineBundle.getRawLayout(), SwPostProcess::FXAAPC::sStages, 0, mResources.mFXAAPushConstants
        );

        vk::Extent3D drawExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
        cmd.dispatch(
            SwHelper::fastDivCeil(drawExtent.width, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            SwHelper::fastDivCeil(drawExtent.height, SwRenderer::MAX_2D_WORKGROUP_THREADS),
            1
        );
    });
    staticDeps.clear();
}

void SwPostProcess::System::reInitializeOnResize() {
    // The draw image is recreated on resize, so rebind every view onto it and refresh the texel size FXAA samples with.
    vk::ImageView drawImageView = SwRenderer::sRendererContext.mSwapchain->getDrawImage().getRawMainImageView();

    mResources.mTonemapDescriptorSet.writeImage(0, drawImageView, nullptr, vk::ImageLayout::eGeneral);
    mResources.mTonemapDescriptorSet.pushWrites();

    mResources.mFXAADescriptorSet.writeImage(0, drawImageView, nullptr, vk::ImageLayout::eGeneral);
    mResources.mFXAADescriptorSet.writeImage(2, drawImageView, nullptr, vk::ImageLayout::eGeneral);
    mResources.mFXAADescriptorSet.pushWrites();

    vk::Extent2D drawExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D();
    mResources.mFXAAPushConstants.mInverseScreenSize = glm::vec2(1.f / drawExtent.width, 1.f / drawExtent.height);
}
