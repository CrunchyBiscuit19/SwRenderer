#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwShader.h>
#include <Scene/SwPick.h>
#include <Scene/SwScene.h>

#include <ranges>

SwPick::System::System(SwScene& scene) : SwSystem(scene) {}

void SwPick::System::initializeResources() {
    mResources.mReadbackBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        sizeof(SwPick::ReadbackData)
    );

    mResources.mReadbackDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute);
    mResources.mReadbackDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mResources.mReadbackDescriptorLayout);

    mResources.mDrawPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, SwPick::DrawPC::getRange());

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
    mResources.mDrawPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline(drawPipelineOptions);

    mResources.mReadbackPipelineLayout =
        SwPipelineFactory::createPipelineLayout(mResources.mReadbackDescriptorLayout.getRawLayout(), SwPick::ReadbackPC::getRange());

    SwShader workShader = SwShaderFactory::createShader(PICK_READBACK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mReadbackPipelineBundle =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mResources.mReadbackPipelineLayout.getRawLayout()});

    mResources.mDrawPushConstants.mSceneVertexBuffer = mScene.getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneNodeTransformsBuffer = mScene.getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneInstancesBuffer = mScene.getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer().getDeviceAddress().value();
    mResources.mDrawPushConstants.mPostCullRenderItemsBuffer = 0;

    mResources.mReadbackPushConstants.mReadbackBuffer = mResources.mReadbackBuffer.getDeviceAddress().value();

    sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_DELETE] && mResources.mClickedInstance != nullptr && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mResources.mClickedInstance->markDelete();
        }
    });

    reInitializeOnResize();
}

void SwPick::System::refreshBatchDependencies() {
    SwDependency deps;

    deps.mWriteImages.emplace_back(&mResources.mReadbackImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mResources.mDepthImage, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&mResources.mDepthImage, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            deps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.mPasses[SwPass::Type::PickDraw].setDeps(std::move(deps));
}

void SwPick::System::initializePasses() {
    SwDependency deps;

    // Pick Draw
    deps.mWriteImages.emplace_back(&mResources.mReadbackImage, SwDependency::ImageDepType::ColorAttachmentWrite);
    deps.mWriteImages.emplace_back(&mResources.mDepthImage, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadImages.emplace_back(&mResources.mDepthImage, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneVisibleRenderInstancesInstanceIndexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    deps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
        for (auto& batch : batchType | std::views::values) {
            deps.mReadBuffers.emplace_back(&batch.getPostCullRenderItemsBuffer(), SwDependency::BufferDepType::IndirectRead);
        }
    }
    mScene.insertPass(SwPass::Type::PickDraw, std::move(deps), [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = mResources.mReadbackImage.generateRenderingAttachment();
        vk::RenderingAttachmentInfo depthAttachment = mResources.mDepthImage.generateRenderingAttachment();
        const vk::RenderingInfo renderInfo = SwPass::generateRenderingInfo(sRendererContext.mSwapchain->getWindowExtent(), colorAttachment, depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(mResources.mDrawPipelineBundle.getBindPoint(), mResources.mDrawPipelineBundle.getRawPipeline());
        SwPass::setViewportScissors(cmd, vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1});
        cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getRawBuffer(), 0, vk::IndexType::eUint32);
        for (auto& batchType : mScene.getBatchTypes() | std::views::values) {
            for (auto& batch : batchType | std::views::values) {
                if (batch.getRenderItems().empty()) {
                    continue;
                }
                mResources.mDrawPushConstants.mPostCullRenderItemsBuffer = batch.getPostCullRenderItemsBuffer().getDeviceAddress().value();
                mResources.mDrawPushConstants.mPerFrameBuffer = sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
                cmd.pushConstants<SwPick::DrawPC>(mResources.mDrawPipelineBundle.getRawLayout(), SwPick::DrawPC::sStages, 0, mResources.mDrawPushConstants);
                cmd.drawIndexedIndirectCount(
                    batch.getPostCullRenderItemsBuffer().getRawBuffer(),
                    0,
                    batch.getPostCullRenderItemsCountBuffer().getRawBuffer(),
                    0,
                    SwScene::DRAW_MAX_RENDER_ITEMS,
                    sizeof(SwRenderItem)
                );
            }
        }

        cmd.endRendering();
    });
    deps.clear();

    // Pick Readback
    deps.mReadImages.emplace_back(&mResources.mReadbackImage, SwDependency::ImageDepType::FragmentShaderSampledRead);
    deps.mWriteBuffers.emplace_back(&mResources.mReadbackBuffer, SwDependency::BufferDepType::ComputeStorageWrite);
    mScene.insertPass(
        SwPass::Type::PickReadback,
        std::move(deps),
        [&](vk::CommandBuffer cmd) {
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
    deps.clear();

    // Pick Work
    deps.mReadBuffers.emplace_back(&mResources.mReadbackBuffer, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
    mScene.insertPass(
        SwPass::Type::PickReadback,
        std::move(deps),
        [&](vk::CommandBuffer cmd) {
            glm::uvec2 read(0);
            std::memcpy(
                glm::value_ptr(read),
                static_cast<char*>(mResources.mReadbackBuffer.getMappedPtr()) + sizeof(SwPick::ReadbackData::mCoords),
                sizeof(SwPick::ReadbackData::mRead)
            );

            if (read.x == 0 || read.y == 0) {
                mResources.mClickedInstance = nullptr;
                return;
            }
            std::uint32_t modelId = read.x - 1;
            if (mScene.getAssets().contains(modelId)) {
                mResources.mClickedInstance = nullptr;
                return;
            }
            SwAsset& selectedAsset = mScene.getAssets()[modelId];

            std::uint32_t localInstanceIndex = (read.y - 1) - selectedAsset.mFirstInstanceInScene;
            mResources.mClickedInstance = &selectedAsset.getInstances()[localInstanceIndex];
        },
        true
    );
    deps.clear();
}

void SwPick::System::reInitializeOnResize() {
    vk::Extent3D imageExtent = vk::Extent3D{sRendererContext.mSwapchain->getWindowExtent(), 1};
    mResources.mReadbackImage = SwImageFactory::createColorImage2D(
        nullptr,
        vk::Format::eR32G32Uint,
        imageExtent,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        false
    );

    mResources.mDepthImage =
        SwImageFactory::createDepthImage2D(nullptr, SwSwapchain::DEPTH_FORMAT, imageExtent, vk::ImageUsageFlagBits::eDepthStencilAttachment);

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mResources.mReadbackImage.emitTransition(cmd, SwDependency::ImageDepType::ColorAttachmentWrite);
        mResources.mDepthImage.emitTransition(cmd, SwDependency::ImageDepType::DepthAttachmentReadWrite);
    });

    mResources.mReadbackDescriptorSet.writeImage(
        0, mResources.mReadbackImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mResources.mReadbackDescriptorSet.pushWrites();
}

void SwPick::System::changePickOperation() {
    switch (mResources.mImguizmoOperation) {
        case ImGuizmo::TRANSLATE:
            mResources.mImguizmoOperation = ImGuizmo::ROTATE;
            break;
        case ImGuizmo::ROTATE:
            mResources.mImguizmoOperation = ImGuizmo::SCALEU;
            break;
        case ImGuizmo::SCALEU:
            mResources.mImguizmoOperation = ImGuizmo::TRANSLATE;
            break;
        default:
            mResources.mImguizmoOperation = ImGuizmo::TRANSLATE;
    }
}

void SwPick::System::generatePickFrame() {
    if (mResources.mClickedInstance == nullptr) return;

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetGizmoSizeClipSpace(SwPick::PICK_IMGUIZMO_SIZE);

    ImGuizmo::SetRect(
        0,
        0,
        static_cast<float>(sRendererContext.mSwapchain->getWindowExtent().width),
        static_cast<float>(sRendererContext.mSwapchain->getWindowExtent().height)
    );

    ImGuizmo::Manipulate(
        glm::value_ptr(mScene.getCamera().getPerspective().mView),
        glm::value_ptr(mScene.getCamera().getPerspective().mProj),
        mResources.mImguizmoOperation,
        ImGuizmo::WORLD,
        glm::value_ptr(mResources.mClickedInstance->getData().mTransformMatrix)
    );

    if (ImGuizmo::IsUsing()) {
        mScene.getAsset(mResources.mClickedInstance->getAssetId()).setReloadInstancesFlag(true);
    }
}

bool SwPick::System::isPicked() {
    return (
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) && !sRendererContext.mScene->getCamera().getRelativeMode() && !ImGui::GetIO().WantCaptureMouse
    );
}

void SwPick::System::resize() {
    mResources.mReadbackImage.destroy();
    mResources.mDepthImage.destroy();
    reInitializeOnResize();
}