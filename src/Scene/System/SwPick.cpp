#include <Data/SwMaterial.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwRenderer.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/System/SwPick.h>
#include <Scene/SwScene.h>
#include <quill/LogMacros.h>

SwPick::System::System(SwScene& scene) : SwSystem(scene) {}

void SwPick::System::initializeResources() {
    mResources.mReadbackBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, sizeof(SwPick::ReadbackData), true
    );

    mResources.mReadbackDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute
    );
    mResources.mReadbackDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mReadbackDescriptorLayout);

    mResources.mDrawPipelineLayout =
        SwPipelineFactory::createPipelineLayout(SwMaterialResources::sMaterialResourcesDescriptorLayout.getRawLayout(), SwPick::DrawPC::getRange());

    SwShader drawVertexShader = SwShaderFactory::createShader(PICK_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader drawFragmentShader = SwShaderFactory::createShader(PICK_DRAW_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    noBlendState.blendEnable = VK_FALSE;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getRawModule();
    drawPipelineOptions.mFragmentShader = drawFragmentShader.getRawModule();
    drawPipelineOptions.mLayout = mResources.mDrawPipelineLayout.getRawLayout();
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

    drawPipelineOptions.mVertexEntryPoint = std::string(SwPick::PICK_DRAW_OPAQUE_ENTRY_POINT);
    drawPipelineOptions.mFragmentEntryPoint = std::string(SwPick::PICK_DRAW_OPAQUE_ENTRY_POINT);
    mResources.mDrawOpaqueTransparentPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(drawPipelineOptions);

    drawPipelineOptions.mVertexEntryPoint = std::string(SwPick::PICK_DRAW_MASKED_ENTRY_POINT);
    drawPipelineOptions.mFragmentEntryPoint = std::string(SwPick::PICK_DRAW_MASKED_ENTRY_POINT);
    mResources.mDrawMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(drawPipelineOptions);

    mResources.mReadbackPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mReadbackDescriptorLayout.getRawLayout(), SwPick::ReadbackPC::getRange());

    SwShader workShader = SwShaderFactory::createShader(PICK_READBACK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mReadbackPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mResources.mReadbackPipelineLayout.getRawLayout()});

    SwRenderer::sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_DELETE] && mSelectedInstance != nullptr && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mSelectedInstance->markDelete();
        }
    });

    reInitializeOnResize();
}

void SwPick::System::initializePasses() {
    SwDependency staticDeps;

    // Pick Draw
    staticDeps.mWriteImages.emplace_back(&mResources.mReadbackImage, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    staticDeps.mWriteImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadImages.emplace_back(&SwRenderer::sRendererContext.mSwapchain->getDepthImage(), SwDependency::ImageDepType::DepthAttachmentReadWrite);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::PickDraw, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = mResources.mReadbackImage.generateRenderingAttachment();
        vk::RenderingAttachmentInfo depthAttachment = SwRenderer::sRendererContext.mSwapchain->getDepthImage().generateRenderingAttachment();
        const vk::RenderingInfo renderInfo =
            SwPass::generateRenderingInfo(SwRenderer::sRendererContext.mSwapchain->getWindowExtent2D(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        SwPass::setViewportScissors(cmd, SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D());

        cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getRawBuffer(), 0, vk::IndexType::eUint32);
        cmd.bindDescriptorSets(
            mResources.mDrawOpaqueTransparentPipelineBundle.getBindPoint(),  // Same across opaque / transparent / masked
            mResources.mDrawPipelineLayout.getRawLayout(),
            0,
            mScene.getSceneMaterialResourcesDescriptorSet().getRawSet(),
            nullptr
        );

        // Draw only the culled commands, mirroring what the geometry pass put in the shared depth image. 
        // Opaque / transparent share one pipeline, while masked uses the discard one.
        auto drawBatches = [&](auto&& batches, SwGraphicsPipelineBundle& pipeline, bool early) {
            cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getRawPipeline());

            for (auto& batch : batches) {
                if (batch.getRcs().empty()) {
                    continue;
                }

                auto& rcBuffer = early ? batch.getEarlyRcsBuffer() : batch.getFinalRcsBuffer();
                auto& countBuffer = early ? batch.getEarlyRcsCount() : batch.getFinalRcsCount();

                mResources.mDrawPushConstants.mDrawRcsBuffer = rcBuffer.getDeviceAddress().value();
                cmd.pushConstants<SwPick::DrawPC>(mResources.mDrawPipelineLayout.getRawLayout(), SwPick::DrawPC::sStages, 0, mResources.mDrawPushConstants);

                cmd.drawIndexedIndirectCount(
                    rcBuffer.getRawBuffer(),
                    0,
                    countBuffer.getRawBuffer(),
                    0,
                    static_cast<std::uint32_t>(batch.getRcs().size()),
                    sizeof(SwRenderCommand)
                );
            }
        };

        drawBatches(mScene.getBatchIt(SwMaterial::Type::Opaque), mResources.mDrawOpaqueTransparentPipelineBundle, true);
        drawBatches(mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Transparent), mResources.mDrawOpaqueTransparentPipelineBundle, false);
        drawBatches(mScene.getBatchIt(SwMaterial::Type::Mask), mResources.mDrawMaskedPipelineBundle, false);

        cmd.endRendering();
    });
    staticDeps.clear();

    // Pick Readback
    staticDeps.mReadImages.emplace_back(&mResources.mReadbackImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    staticDeps.mWriteBuffers.emplace_back(&mResources.mReadbackBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    mScene.insertPass(
        SwPass::Type::PickReadback,
        std::move(staticDeps),
        [&](vk::CommandBuffer cmd) {
            glm::ivec2 mousePos;
            SDL_GetMouseState(&mousePos.x, &mousePos.y);
            mResources.mReadbackBuffer.copyFromUnchecked(glm::value_ptr(mousePos), sizeof(SwPick::ReadbackData::mCoords));

            cmd.bindPipeline(mResources.mReadbackPipelineBundle.getBindPoint(), mResources.mReadbackPipelineBundle.getRawPipeline());

            cmd.bindDescriptorSets(
                mResources.mReadbackPipelineBundle.getBindPoint(),
                mResources.mReadbackPipelineBundle.getRawLayout(),
                0,
                mResources.mReadbackDescriptorSet.getRawSet(),
                nullptr
            );

            cmd.pushConstants<SwPick::ReadbackPC>(
                mResources.mReadbackPipelineBundle.getRawLayout(), SwPick::ReadbackPC::sStages, 0, mResources.mReadbackPushConstants
            );

            cmd.dispatch(1, 1, 1);
        },
        true
    );
    staticDeps.clear();

    // Pick Work
    staticDeps.mReadBuffers.emplace_back(&mResources.mReadbackBuffer, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
    mScene.insertPass(
        SwPass::Type::PickWork,
        std::move(staticDeps),
        [&](vk::CommandBuffer cmd) {
            glm::uvec2 read(0);
            std::memcpy(
                glm::value_ptr(read),
                static_cast<char*>(mResources.mReadbackBuffer.getMappedPtr()) + sizeof(SwPick::ReadbackData::mCoords),
                sizeof(SwPick::ReadbackData::mRead)
            );

            if (read.x == 0 || read.y == 0) {
                mSelectedInstance = nullptr;
                return;
            }
            std::uint32_t assetId = read.x - 1;
            if (!mScene.getAssets().contains(assetId)) {
                mSelectedInstance = nullptr;
                return;
            }
            SwAsset& selectedAsset = mScene.getAssets()[assetId];

            std::uint32_t localInstanceIndex = (read.y - 1) - selectedAsset.mFirstInstanceInScene;
            mSelectedInstance = &selectedAsset.getInstances()[localInstanceIndex];
        },
        true
    );
    staticDeps.clear();
}

void SwPick::System::initializePushConstants() { mResources.mReadbackPushConstants.mReadbackBuffer = mResources.mReadbackBuffer.getDeviceAddress().value(); }

void SwPick::System::reInitializeOnResize() {
    vk::Extent3D imageExtent = SwRenderer::sRendererContext.mSwapchain->getWindowExtent3D();
    mResources.mReadbackImage = SwImageFactory::createColorImage2D(
        "ReadbackImage",
        nullptr,
        vk::Format::eR32G32Uint,
        imageExtent,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        false
    );

    SwRenderer::sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mResources.mReadbackImage.emitTransition(cmd, SwDependency::ImageDepType::ColorAttachmentReadWrite);
    });

    mResources.mReadbackDescriptorSet.writeImage(0, mResources.mReadbackImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);
    mResources.mReadbackDescriptorSet.pushWrites();
}

void SwPick::System::refreshDynamicDependencies() {
    SwDependency dynamicDeps;

    // Pick Draw
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead
    );
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
        dynamicDeps.mReadBuffers.emplace_back(&batch.getEarlyRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
    }
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask, SwMaterial::Type::Transparent)) {
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
    }
    mScene.mPasses[SwPass::Type::PickDraw].setDynamicDeps(std::move(dynamicDeps));
    dynamicDeps.clear();
}

void SwPick::System::refreshPushConstants() {
    mResources.mDrawPushConstants.mSceneVertexBuffer = mScene.getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneMaterialConstantsBuffer = mScene.getSceneMaterialConstantsBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneDrawRisIndicesBuffer =
        mScene.getSceneDrawRisIndicesBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
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
    if (mSelectedInstance == nullptr) return;

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetGizmoSizeClipSpace(SwPick::PICK_IMGUIZMO_SIZE);

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
        glm::value_ptr(mSelectedInstance->getData().mTransformMatrix)
    );

    if (ImGuizmo::IsUsing()) {
        mScene.getAsset(mSelectedInstance->getAssetId()).setReloadInstancesFlag(true);
    }
}

bool SwPick::System::isPicked() {
    return (
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) && !SwRenderer::sRendererContext.mScene->getCamera().getRelativeMode() &&
        !ImGui::GetIO().WantCaptureMouse
    );
}