#include <Data/SwMaterial.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwInput.h>
#include <System/SwPick.h>
#include <quill/LogMacros.h>

SwPick::System::System(SwScene& scene) : SwSystem(scene) {}

void SwPick::System::drawBatches(
    vk::CommandBuffer cmd, std::array<std::optional<SwMaterial::Type>, SwMaterial::NUM_TYPES> matTypes, SwGraphicsPipelineBundle& pipeline, bool early
) {
    SwAllocatedBuffer& rcsBuffer = early ? mScene.getEarlyRcsBuffer() : mScene.getLateRcsBuffer();
    SwAllocatedBuffer& rcsCount = early ? mScene.getEarlyRcsCount() : mScene.getLateRcsCount();
    cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());

    for (auto& batch : mScene.getBatchIt(matTypes)) {
        if (batch.getRcsSize() == 0) continue;

        // SV_DrawIndex is relative to each indirect call, so offset the pointer to the batch base to keep shader indexing aligned with the draw offset.
        mResources.mDrawPushConstants.mRcsBuffer = rcsBuffer + batch.getRcsIndex() * sizeof(SwRenderCommand);
        cmd.pushConstants<DrawPC>(mResources.mDrawPipelineLayout.getHandle(), DrawPC::sStages, 0, mResources.mDrawPushConstants);

        cmd.drawIndexedIndirectCount(
            rcsBuffer.getHandle(),
            batch.getRcsIndex() * sizeof(SwRenderCommand),
            rcsCount.getHandle(),
            batch.getBatchIndex() * sizeof(std::uint32_t),
            static_cast<std::uint32_t>(batch.getRcsSize()),
            sizeof(SwRenderCommand)
        );
    }
}

void SwPick::System::initializeResources() {
    mResources.mReadbackBuffer = SwBufferFactory::createAllocatedBuffer(
        "PickReadbackBuffer", vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, sizeof(ReadbackData), true
    );

    mResources.mReadbackDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "PickReadbackDescriptorSetLayout", {{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mReadbackDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("PickReadbackDescriptorSet", mResources.mReadbackDescriptorLayout);

    mResources.mDrawPipelineLayout = SwPipelineFactory::createPipelineLayout(
        "PickDrawPipelineLayout",
        {SwMaterialResources::sMaterialSamplersDescriptorLayout.getHandle(), SwMaterialResources::sMaterialTexturesDescriptorLayout.getHandle()},
        DrawPC::getRange()
    );

    SwShader drawVertexShader = SwShaderFactory::createShader("PickDrawVertexShaderModule", DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader drawFragmentShader = SwShaderFactory::createShader("PickDrawFragmentShaderModule", DRAW_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    noBlendState.blendEnable = VK_FALSE;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getHandle();
    drawPipelineOptions.mFragmentShader = drawFragmentShader.getHandle();
    drawPipelineOptions.mLayout = mResources.mDrawPipelineLayout.getHandle();
    drawPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    drawPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    drawPipelineOptions.mCullMode = vk::CullModeFlagBits::eBack;
    drawPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    drawPipelineOptions.mMultisamplingEnabled = false;
    drawPipelineOptions.mSampleShadingEnabled = false;
    drawPipelineOptions.mColorAttachments = std::vector<std::pair<vk::Format, vk::PipelineColorBlendAttachmentState>>{{vk::Format::eR32G32Uint, noBlendState}};
    drawPipelineOptions.mDepthFormat = SwSwapchain::DEPTH_FORMAT;
    drawPipelineOptions.mDepthTestEnabled = true;
    drawPipelineOptions.mDepthWriteEnabled = true;
    drawPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;

    drawPipelineOptions.mVertexEntryPoint = std::string(DRAW_OPAQUE_ENTRY_POINT);
    drawPipelineOptions.mFragmentEntryPoint = std::string(DRAW_OPAQUE_ENTRY_POINT);
    mResources.mDrawOpaqueTransparentPipelineBundle =
        SwGraphicsPipelineFactory::createGraphicsPipeline("PickDrawOpaqueTransparentPipeline", drawPipelineOptions);

    drawPipelineOptions.mVertexEntryPoint = std::string(DRAW_MASKED_ENTRY_POINT);
    drawPipelineOptions.mFragmentEntryPoint = std::string(DRAW_MASKED_ENTRY_POINT);
    mResources.mDrawMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("PickDrawMaskedPipeline", drawPipelineOptions);

    mResources.mReadbackPipelineLayout =
        SwPipelineFactory::createPipelineLayout("PickReadbackPipelineLayout", mResources.mReadbackDescriptorLayout.getHandle(), ReadbackPC::getRange());

    SwShader readbackShader = SwShaderFactory::createShader("PickReadbackShaderModule", READBACK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mReadbackPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("PickReadbackPipeline", {readbackShader.getHandle(), mResources.mReadbackPipelineLayout.getHandle()});

    reInitializeOnResize();
}

void SwPick::System::initializePasses() {
    // Pick Draw
    mScene.insertPass(SwPass::Type::PickDraw, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = mResources.mReadbackImage.generateRenderingAttachment();
        vk::RenderingAttachmentInfo depthAttachment = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo =
            SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());

        cmd.bindIndexBuffer(mScene.getIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);
        cmd.bindDescriptorSets(
            mResources.mDrawOpaqueTransparentPipelineBundle.getBindPoint(),
            mResources.mDrawPipelineLayout.getHandle(),
            0,
            {mScene.getMaterialSamplersDescriptorSet().getHandle(), mScene.getMaterialTexturesDescriptorSet().getHandle()},
            nullptr
        );

        drawBatches(cmd, {SwMaterial::Type::Opaque}, mResources.mDrawOpaqueTransparentPipelineBundle, true);
        drawBatches(cmd, {SwMaterial::Type::Opaque, SwMaterial::Type::Transparent}, mResources.mDrawOpaqueTransparentPipelineBundle, false);
        drawBatches(cmd, {SwMaterial::Type::Mask}, mResources.mDrawMaskedPipelineBundle, false);

        cmd.endRendering();
    });

    // Pick Readback
    mScene.insertPass(
        SwPass::Type::PickReadback,
        [&](vk::CommandBuffer cmd) {
            glm::vec2 mousePosF;
            SDL_GetMouseState(&mousePosF.x, &mousePosF.y);
            glm::ivec2 mousePos = glm::ivec2(mousePosF);
            mResources.mReadbackBuffer.copyFromUnchecked(glm::value_ptr(mousePos), sizeof(ReadbackData::mCoords));

            cmd.bindPipeline(mResources.mReadbackPipelineBundle.getBindPoint(), mResources.mReadbackPipelineBundle.getPipelineHandle());

            cmd.bindDescriptorSets(
                mResources.mReadbackPipelineBundle.getBindPoint(),
                mResources.mReadbackPipelineBundle.getLayoutHandle(),
                0,
                mResources.mReadbackDescriptorSet.getHandle(),
                nullptr
            );

            cmd.pushConstants<ReadbackPC>(mResources.mReadbackPipelineBundle.getLayoutHandle(), ReadbackPC::sStages, 0, mResources.mReadbackPushConstants);

            cmd.dispatch(1, 1, 1);
        },
        true
    );

    // Pick Select
    mScene.insertPass(
        SwPass::Type::PickSelect,
        [&](vk::CommandBuffer cmd) {
            glm::uvec2 read(0);
            std::memcpy(
                glm::value_ptr(read), static_cast<char*>(mResources.mReadbackBuffer.getMappedPtr()) + sizeof(ReadbackData::mCoords), sizeof(ReadbackData::mRead)
            );

            if (read.x == 0 || read.y == 0) {
                mSelectedInstanceId.reset();
                return;
            }
            std::uint32_t assetId = read.x - 1;
            if (!mScene.getAssets().contains(assetId)) {
                mSelectedInstanceId.reset();
                return;
            }
            SwAsset& selectedAsset = mScene.getAssets()[assetId];

            std::uint32_t localInstanceIndex = (read.y - 1) - selectedAsset.mFirstInstanceInScene;
            mSelectedInstanceId = selectedAsset.getInstanceIds()[localInstanceIndex];
        },
        true
    );
}

void SwPick::System::reInitializeOnResize() {
    vk::Extent3D imageExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
    mResources.mReadbackImage = SwImageFactory::createColorImage2D(
        "PickReadbackImage",
        vk::Format::eR32G32Uint,
        imageExtent,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        false
    );

    SwRenderer::sRendererContext.mImmSubmit->addCallback(SwQueueType::Graphics, [this](vk::CommandBuffer cmd) {
        mResources.mReadbackImage.emitTransition(cmd, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    });

    mResources.mReadbackDescriptorSet.writeImage(0, mResources.mReadbackImage.getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mReadbackDescriptorSet.pushWrites();
}

void SwPick::System::refreshDataUsage() {
    // Pick Draw
    {
        mResources.mDrawPushConstants.mVertexBuffer = mScene.getVertexBuffer();
        mResources.mDrawPushConstants.mMaterialConstantsBuffer = mScene.getMaterialConstantsBuffer();
        mResources.mDrawPushConstants.mNodeTransformsBuffer = mScene.getNodeTransformsBuffer();
        mResources.mDrawPushConstants.mInstancesBuffer = mScene.getInstancesBuffer();
        mResources.mDrawPushConstants.mRisIndicesBuffer = mScene.getRisIndicesBuffer();
        mResources.mDrawPushConstants.mFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer();

        SwDependency& d = mScene.mPasses[SwPass::Type::PickDraw].getDeps();
        d.clear();
        d.mWriteImages.emplace_back(&mResources.mReadbackImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
        d.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        d.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
        d.mReadBuffers.emplace_back(&mScene.getVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(&mScene.getRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
        d.mReadBuffers.emplace_back(
            &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getDataBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead
        );
        d.mReadBuffers.emplace_back(&mScene.getIndexBuffer(), SwDependency::BufferDepType::IndexRead);
        d.mReadBuffers.emplace_back(&mScene.getEarlyRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&mScene.getEarlyRcsCount(), SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&mScene.getLateRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
        d.mReadBuffers.emplace_back(&mScene.getLateRcsCount(), SwDependency::BufferDepType::IndirectRead);
    }

    // Pick Readback
    {
        mResources.mReadbackPushConstants.mReadbackBuffer = mResources.mReadbackBuffer;

        SwDependency& d = mScene.mPasses[SwPass::Type::PickReadback].getDeps();
        d.clear();
        d.mReadImages.emplace_back(&mResources.mReadbackImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
        d.mWriteBuffers.emplace_back(&mResources.mReadbackBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    }

    // Pick Select
    {
        SwDependency& d = mScene.mPasses[SwPass::Type::PickSelect].getDeps();
        d.clear();
        d.mReadBuffers.emplace_back(&mResources.mReadbackBuffer, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
    }
}

void SwPick::System::refresh() {
    SwInput::System& input = mScene.getInputSystem();
    if (!ImGui::GetIO().WantCaptureKeyboard && input.wasTriggered(SwInput::DELETE_INSTANCE) && mSelectedInstanceId.has_value() &&
        mScene.getInstances().contains(*mSelectedInstanceId)) {
        mScene.getInstance(*mSelectedInstanceId).markDelete();
    }
    SwSystem::refresh();
}

void SwPick::System::changePickOperation() {
    switch (mImguizmoOperation) {
        case ImGuizmo::TRANSLATE:
            mImguizmoOperation = ImGuizmo::ROTATE;
            break;
        case ImGuizmo::ROTATE:
            mImguizmoOperation = ImGuizmo::SCALEU;
            break;
        case ImGuizmo::SCALEU:
            mImguizmoOperation = ImGuizmo::TRANSLATE;
            break;
        default:
            mImguizmoOperation = ImGuizmo::TRANSLATE;
    }
}

void SwPick::System::generatePickFrame() {
    if (!mSelectedInstanceId.has_value() || !mScene.getInstances().contains(*mSelectedInstanceId)) return;
    SwInstance& selectedInstance = mScene.getInstance(*mSelectedInstanceId);

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetGizmoSizeClipSpace(IMGUIZMO_SIZE);

    ImGuizmo::SetRect(
        0,
        0,
        static_cast<float>(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().width),
        static_cast<float>(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D().height)
    );

    ImGuizmo::Manipulate(
        glm::value_ptr(mScene.getCamera().getPerspective().getView()),
        glm::value_ptr(mScene.getCamera().getPerspective().getProjGL()),
        mImguizmoOperation,
        ImGuizmo::WORLD,
        glm::value_ptr(selectedInstance.getData().mTransformMatrix)
    );

    if (ImGuizmo::IsUsing()) {
        mScene.getAsset(selectedInstance.getAssetId()).setReloadInstancesFlag(true);
    }
}

bool SwPick::System::isPicked() {
    return (
        mScene.getInputSystem().isActive(SwInput::SELECT_OBJECT) && !mScene.getCamera().getRelativeMode() &&
        !ImGui::GetIO().WantCaptureMouse
    );
}