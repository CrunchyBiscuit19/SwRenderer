#include <Scene/SwSkybox.h>
#include <Scene/SwScene.h>
#include <Renderer/SwSwapchain.h>
#include <Renderer/SwImmSubmit.h>
#include <Resource/SwShader.h>

#include <stb_image.h>

SwSkybox::System::System(SwScene& scene) : SwSystem(scene) {}

void SwSkybox::System::initializeResources() {
    mResources.mWorkSampler = SwSamplerFactory::createSampler(vk::SamplerCreateInfo());

    mResources.mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eCombinedImageSampler, 1}}, vk::ShaderStageFlagBits::eFragment);
    mResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mWorkDescriptorLayout);

    mResources.mWorkPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mWorkDescriptorLayout.getRawLayout(), SwSkybox::WorkPC::getRange());

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
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        skyboxVertexSize
    );

    SwStagingBuffer skyboxVertexStagingBuffer = SwBufferFactory::createStagingBuffer(skyboxVertexSize);
    std::memcpy(skyboxVertexStagingBuffer.getMappedPointer(), mResources.mWorkVertices.data(), skyboxVertexSize);

    vk::BufferCopy skyboxVertexCopy{};
    skyboxVertexCopy.dstOffset = 0;
    skyboxVertexCopy.srcOffset = 0;
    skyboxVertexCopy.size = skyboxVertexSize;

    sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        skyboxVertexStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        mResources.mWorkVertexBuffer.copyFrom(cmd, skyboxVertexStagingBuffer, skyboxVertexCopy, skyboxVertexCopy.size);
    });

    mResources.mWorkPushConstants.mWorkVertexBuffer = mResources.mWorkVertexBuffer.getDeviceAddress().value();

    reinitializeOnUpdate(SKYBOX_DEFAULT_DIRECTORY_PATH);
}

void SwSkybox::System::initializePasses() {
    SwDependency deps;

    // Skybox
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&mResources.mWorkImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    deps.mReadBuffers.emplace_back(&mResources.mWorkVertexBuffer, SwDependency::BufferDepType::VertexShaderStorageRead);

    mScene.insertPass(SwPass::Type::SkyboxWork, std::move(deps), [&](vk::CommandBuffer cmd) {
        const vk::RenderingAttachmentInfo colorAttachment = sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
        const vk::RenderingAttachmentInfo depthAttachment = sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawPipeline());
        cmd.bindDescriptorSets(
            mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getRawLayout(), 0, mResources.mWorkDescriptorSet.getRawSet(), nullptr
        );
        SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
        mResources.mWorkPushConstants.mPerFrameBuffer = sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
        cmd.pushConstants<SwSkybox::WorkPC>(mResources.mWorkPipelineBundle.getRawLayout(), SwSkybox::WorkPC::sStages, 0, mResources.mWorkPushConstants);
        cmd.draw(SwSkybox::NUM_SKYBOX_VERTICES, 1, 0, 0);
        sRendererContext.mStats->mDrawCallCount++;

        cmd.endRendering();
    });
    deps.clear();
}

void SwSkybox::System::reinitializeOnUpdate(std::optional<std::filesystem::path> newLoadDir) {
    mLoadFromDir = newLoadDir;
    if (!mLoadFromDir.has_value()) {
        return;
    }

    std::vector<std::filesystem::path> skyboxImagePaths = {
        mLoadFromDir.value() / "px.png",
        mLoadFromDir.value() / "nx.png",
        mLoadFromDir.value() / "py.png",
        mLoadFromDir.value() / "ny.png",
        mLoadFromDir.value() / "pz.png",
        mLoadFromDir.value() / "nz.png"
    };
    std::vector<std::byte> skyboxImageData;

    int width = 0;
    int height = 0;
    int nrChannels = 0;
    for (auto& path : skyboxImagePaths) {
        int faceWidth = 0;
        int faceHeight = 0;
        int faceChannels = 0;
        if (unsigned char* data = stbi_load(path.string().c_str(), &faceWidth, &faceHeight, &faceChannels, 4)) {
            if (width == 0 && height == 0) {
                width = faceWidth;
                height = faceHeight;
            }
            const std::size_t imageSize = static_cast<std::size_t>(faceWidth) * static_cast<std::size_t>(faceHeight) * 4;
            const std::size_t offset = skyboxImageData.size();
            skyboxImageData.resize(offset + imageSize);
            std::memcpy(skyboxImageData.data() + offset, data, imageSize);
            stbi_image_free(data);
        }
    }
    if (width == 0 || height == 0 || skyboxImageData.empty()) {
        return;
    }

    mResources.mWorkImage = SwImageFactory::createColorImageCubemap(
        skyboxImageData.data(),
        vk::Format::eR8G8B8A8Srgb,
        vk::Extent3D{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        vk::ImageUsageFlagBits::eSampled,
        true
    );

    mResources.mWorkDescriptorSet.writeImage(
        0,
        mResources.mWorkImage.getRawMainImageView(),
        mResources.mWorkSampler.getRawSampler(),
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::DescriptorType::eCombinedImageSampler
    );
    mResources.mWorkDescriptorSet.pushWrites();
}
