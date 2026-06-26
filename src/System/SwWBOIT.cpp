#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwWBOIT.h>

SwWBOIT::System::System(SwScene& scene) : SwSystem(scene) {}

void SwWBOIT::System::initializeResources() {
    mResources.mWorkDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "WBOITWorkDescriptorSetLayout", {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eFragment
    );
    mResources.mWorkDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("WBOITWorkDescriptorSet", mResources.mWorkDescriptorLayout);

    mResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout("WBOITWorkPipelineLayout", mResources.mWorkDescriptorLayout.getHandle(), nullptr);

    SwShader wboitVertexShader = SwShaderFactory::createShader("WBOITVertexShaderModule", WBOIT_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader wboitFragmentShader = SwShaderFactory::createShader("WBOITFragmentShaderModule", WBOIT_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

    vk::PipelineColorBlendAttachmentState compositeBlendAttachment{};
    compositeBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    compositeBlendAttachment.blendEnable = VK_TRUE;
    compositeBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    compositeBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    compositeBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    compositeBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    compositeBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    compositeBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions wboitPipelineOptions;
    wboitPipelineOptions.mVertexShader = wboitVertexShader.getHandle();
    wboitPipelineOptions.mFragmentShader = wboitFragmentShader.getHandle();
    wboitPipelineOptions.mLayout = mResources.mWorkPipelineLayout.getHandle();
    wboitPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    wboitPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    wboitPipelineOptions.mCullMode = vk::CullModeFlagBits::eNone;
    wboitPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    wboitPipelineOptions.mMultisamplingEnabled = false;
    wboitPipelineOptions.mSampleShadingEnabled = false;
    wboitPipelineOptions.mColorAttachments =
        std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{SwSwapchain::DRAW_FORMAT, compositeBlendAttachment}};
    wboitPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    wboitPipelineOptions.mDepthTestEnabled = false;
    wboitPipelineOptions.mDepthWriteEnabled = false;
    wboitPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    mResources.mWorkPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("WBOITWorkPipeline", wboitPipelineOptions);

    reInitializeOnResize();
}

void SwWBOIT::System::initializePasses() {
    SwDependency deps;

    // WBOIT Composite
    deps.mReadImages.emplace_back(&mResources.mAccumImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    deps.mReadImages.emplace_back(&mResources.mRvlImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    deps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDrawImage(), SwDependency::ImageDepType::ColorAttachmentReadWrite);
    mScene.insertPass(SwPass::Type::WBOITComposite, std::move(deps), [&](vk::CommandBuffer cmd) {
        const vk::RenderingAttachmentInfo colorAttachment = SwRenderer::sRendererContext.mSwapchain->getDrawImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colorAttachment, nullptr);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getPipelineHandle());
        cmd.bindDescriptorSets(
            mResources.mWorkPipelineBundle.getBindPoint(), mResources.mWorkPipelineBundle.getLayouthandle(), 0, mResources.mWorkDescriptorSet.getHandle(), nullptr
        );
        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());
        cmd.draw(SwSwapchain::NUM_FULLSCREEN_QUAD_VERTICES, 1, 0, 0);
        SwRenderer::sRendererContext.mStats->mNumDrawCall++;

        cmd.endRendering();
    });
    deps.clear();
}

void SwWBOIT::System::reInitializeOnResize() {
    mResources.mAccumImage = SwImageFactory::createColorImage2D(
        "AccumImage",
        nullptr,
        SwSwapchain::DRAW_FORMAT,
        SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D(),
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        false
    );
    mResources.mRvlImage = SwImageFactory::createColorImage2D(
        "RvlImage",
        nullptr,
        SwWBOIT::RVL_FORMAT,
        SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D(),
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        false,
        SwWBOIT::RVL_CLEAR_VALUE
    );

    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mResources.mAccumImage.emitTransition(cmd, SwDependency::ImageDepType::ColorAttachmentReadWrite);
        mResources.mRvlImage.emitTransition(cmd, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    });

    mResources.mWorkDescriptorSet.writeImage(
        0, mResources.mAccumImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mWorkDescriptorSet.writeImage(
        1, mResources.mRvlImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal
    );
    mResources.mWorkDescriptorSet.pushWrites();
}