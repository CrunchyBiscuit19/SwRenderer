#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <Scene/SwSkybox.h>
#include <stb_image.h>

SwSkybox::System::System(SwScene& scene) : SwSystem(scene) {}

void SwSkybox::System::initializeResources() {
    mResources.mWorkSampler = SwSamplerFactory::createSampler(vk::SamplerCreateInfo());

    mResources.mWorkDescriptorLayout =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eCombinedImageSampler, 1}}, vk::ShaderStageFlagBits::eFragment);
    mResources.mWorkDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mWorkDescriptorLayout);

    mResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout(mResources.mWorkDescriptorLayout.getRawLayout(), SwSkybox::WorkPC::getRange());

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
    skyboxPipelineOptions.mLayout = mResources.mWorkPipelineLayout.getRawLayout();
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
    mResources.mWorkPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(skyboxPipelineOptions);

    const std::uint32_t skyboxVertexSize = static_cast<std::uint32_t>(mResources.mWorkVertices.size() * sizeof(float));
    mResources.mWorkVertexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        skyboxVertexSize,
        true
    );

    SwStagingBuffer skyboxVertexStagingBuffer = SwBufferFactory::createStagingBuffer(skyboxVertexSize);

    vk::BufferCopy skyboxVertexCopy{};
    skyboxVertexCopy.dstOffset = 0;
    skyboxVertexCopy.srcOffset = 0;
    skyboxVertexCopy.size = skyboxVertexSize;

    SwRenderer::sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        skyboxVertexStagingBuffer.copyFromUnchecked(mResources.mWorkVertices.data(), skyboxVertexCopy.size);
        mResources.mWorkVertexBuffer.copyFrom(cmd, skyboxVertexStagingBuffer, skyboxVertexCopy);
    });

    reinitializeOnUpdate(SKYBOX_DEFAULT_HDR_PATH);
}

void SwSkybox::System::initializePasses() {
    SwDependency deps;

    // Skybox
    deps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    deps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&mResources.mWorkImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    deps.mReadBuffers.emplace_back(&mResources.mWorkVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);

    mScene.insertPass(SwPass::Type::SkyboxWork, std::move(deps), [&](vk::CommandBuffer cmd) {
        const vk::RenderingAttachmentInfo colorAttachment = SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
        const vk::RenderingAttachmentInfo depthAttachment = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawLayout(), 0, mResources.mWorkDescriptorSet.getRawSet(), nullptr
        );
        SwPass::setViewportScissors(cmd, vk::Extent3D{SwRenderer::sRendererContext.mSwapchain->getWindowExtent(), 1});
        cmd.pushConstants<SwSkybox::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwSkybox::WorkPC::sStages, 0, mResources.mWorkPushConstants);
        cmd.draw(SwSkybox::NUM_SKYBOX_VERTICES, 1, 0, 0);
        SwRenderer::sRendererContext.mStats->mDrawCallCount++;

        cmd.endRendering();
    });
    deps.clear();
}

void SwSkybox::System::initializePushConstants() { mResources.mWorkPushConstants.mWorkVertexBuffer = mResources.mWorkVertexBuffer.getDeviceAddress().value(); }

void SwSkybox::System::reinitializeOnUpdate(std::optional<std::filesystem::path> newLoadFile) {
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

    mResources.mWorkImage = SwImageFactory::createColorImage2D(
        data,
        vk::Format::eR32G32B32A32Sfloat,
        vk::Extent3D{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        vk::ImageUsageFlagBits::eSampled,
        true
    );

    stbi_image_free(data);

    mResources.mWorkDescriptorSet.writeImage(
        0,
        mResources.mWorkImage.getRawMainImageView(),
        mResources.mWorkSampler.getRawSampler(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mWorkDescriptorSet.pushWrites();
}

void SwSkybox::System::refreshPushConstants() {
    mResources.mWorkPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
}