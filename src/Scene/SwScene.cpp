#include <Misc/SwHelper.h>
#include <Renderer/SwEvents.h>
#include <Renderer/SwImmSubmit.h>
#include <Renderer/SwSwapchain.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <stb_image.h>

#include <glm/glm.hpp>

std::filesystem::path SwScene::CULL_RESET_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerReset.comp.spv";
std::filesystem::path SwScene::CULL_WORK_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerCull.comp.spv";
std::filesystem::path SwScene::CULL_COMPACT_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerCompact.comp.spv";
std::filesystem::path SwScene::CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "CullerDepthPyramid.comp.spv";
std::filesystem::path SwScene::PICK_DRAW_VERTEX_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "PickerDraw.vert.spv";
std::filesystem::path SwScene::PICK_DRAW_FRAGMENT_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "PickerDraw.frag.spv";
std::filesystem::path SwScene::PICK_WORK_COMPUTE_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "PickerPick.comp.spv";
std::filesystem::path SwScene::SKYBOX_VERTEX_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Skybox.vert.spv";
std::filesystem::path SwScene::SKYBOX_FRAGMENT_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Skybox.frag.spv";
std::filesystem::path SwScene::WBOIT_VERTEX_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Composite.vert.spv";
std::filesystem::path SwScene::WBOIT_FRAGMENT_SHADER_PATH = std::filesystem::path(SHADERS_PATH) / "Composite.frag.spv";

SwRendererContext SwScene::sRendererContext{};

void SwScene::initializeSceneResources() {
    mSceneVertexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_VERTEX_BUFFER_SIZE
    );

    mSceneIndexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, SCENE_INDEX_BUFFER_SIZE
    );

    mSceneMaterialConstantsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_MATERIALS * sizeof(SwMaterialConstants)
    );

    mSceneNodeTransformsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_NODES * sizeof(glm::mat4)
    );

    mSceneInstancesBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_INSTANCES * sizeof(SwInstanceData)
    );

    mSceneBoundsBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_BOUNDS * sizeof(SwBounds)
    );

    mSceneVisibleRenderInstancesInstanceIndexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        SCENE_NUM_RENDER_INSTANCES * sizeof(std::uint32_t)
    );

    mSceneMaterialResourcesDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(
        SwMaterialResources::sMaterialResourcesDescriptorLayout, SwMaterialResources::MAX_TEXTURE_ARRAY_SLOTS
    );

    sRendererContext.mEvents->addEventCallback([this](SDL_Event& e) -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_DELETE] && mPickResources.mClickedInstance != nullptr && e.type == SDL_KEYDOWN && !e.key.repeat) {
            mPickResources.mClickedInstance->markDelete();
        }
    });
}

void SwScene::initializeCullResources() {
    // Push pass
    vk::PushConstantRange resetPushConstantRange = SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::ResetPC));
    mCullResources.mResetPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, resetPushConstantRange);

    SwShader resetShader = SwShaderFactory::createShader(CULL_RESET_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mResetPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({resetShader.getRawModule(), mCullResources.mResetPipelineLayout.getRawLayout()});

    // Depth pyramid pass
    mCullResources.mDepthPyramidDescriptorLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1},
         {1, vk::DescriptorType::eSampledImage, CULL_MAX_DEPTH_PYRAMID_LEVELS},
         {2, vk::DescriptorType::eStorageImage, CULL_MAX_DEPTH_PYRAMID_LEVELS}},
        vk::ShaderStageFlagBits::eCompute
    );

    mCullResources.mDepthPyramidDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mCullResources.mDepthPyramidDescriptorLayout);

    vk::PushConstantRange depthPyramidPushConstantRange =
        SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::DepthPyramidPC));
    mCullResources.mDepthPyramidPipelineLayout =
        SwPipelineFactory::createPipelineLayout({mCullResources.mDepthPyramidDescriptorLayout.getRawLayout()}, depthPyramidPushConstantRange);

    SwShader depthPyramidShader = SwShaderFactory::createShader(CULL_DEPTH_PYRAMID_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mDepthPyramidPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({depthPyramidShader.getRawModule(), mCullResources.mDepthPyramidPipelineLayout.getRawLayout()});

    // Work pass
    mCullResources.mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute);

    mCullResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mCullResources.mWorkDescriptorLayout);

    vk::PushConstantRange workPushConstantRange = SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::WorkPC));
    mCullResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout({mCullResources.mWorkDescriptorLayout.getRawLayout()}, workPushConstantRange);

    SwShader workShader = SwShaderFactory::createShader(CULL_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mWorkPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mCullResources.mWorkPipelineLayout.getRawLayout()});

    // Compact pass
    vk::PushConstantRange compactPushConstantRange =
        SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwCull::CompactPC));
    mCullResources.mCompactPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, compactPushConstantRange);

    SwShader compactShader = SwShaderFactory::createShader(CULL_COMPACT_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mCullResources.mCompactPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({compactShader.getRawModule(), mCullResources.mCompactPipelineLayout.getRawLayout()});

    // Everything that needs to be re-built on resize
    onResizeInitializeCullResources();
}

void SwScene::onResizeInitializeCullResources() {
    // Depth pyramid pass
    std::uint32_t depthPyramidWidth = swHelper::previousPow2(sRendererContext.mSwapchain->getDepthImage().getExtent().width);
    std::uint32_t depthPyramidHeight = swHelper::previousPow2(sRendererContext.mSwapchain->getDepthImage().getExtent().height);
    mCullResources.mDepthPyramidExtent = vk::Extent3D{
        depthPyramidWidth,
        depthPyramidHeight,
        1,
    };
    mCullResources.mDepthPyramidLevels = swHelper::calculateMipMapLevels(mCullResources.mDepthPyramidExtent);
    mCullResources.mDepthPyramidImage = SwImageFactory::createColorImage2D(
        nullptr, vk::Format::eR32Sfloat, mCullResources.mDepthPyramidExtent, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );
    for (std::uint32_t i = 0; i < mCullResources.mDepthPyramidLevels; i++) {
        mCullResources.mDepthPyramidImage.addImageView(
            mCullResources.mDepthPyramidImage.getMainFormat(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, i, 1
        );
    }
    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mCullResources.mDepthPyramidImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );
    });

    mCullResources.mDepthPyramidDescriptorSet.writeImage(
        0,
        sRendererContext.mSwapchain->getDepthImage().getRawMainImageView(),
        nullptr,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::DescriptorType::eSampledImage
    );
    for (std::uint32_t i = 0; i < mCullResources.mDepthPyramidLevels; i++) {
        mCullResources.mDepthPyramidDescriptorSet.writeImage(
            1, mCullResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eSampledImage, i
        );
        mCullResources.mDepthPyramidDescriptorSet.writeImage(
            2, mCullResources.mDepthPyramidImage.getRawOtherImageView(i), nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage, i
        );
    }
    mCullResources.mDepthPyramidDescriptorSet.pushWrites();

    vk::Extent3D depthPyramidExtent = mCullResources.mDepthPyramidImage.getExtent();
    vk::Extent3D depthExtent = sRendererContext.mSwapchain->getDepthImage().getExtent();
    mCullResources.mDepthPyramidPushConstants.mDepthPyramidExtent = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
    mCullResources.mDepthPyramidPushConstants.mDepthFullExtent = glm::uvec2(depthExtent.width, depthExtent.height);
    mCullResources.mDepthPyramidPushConstants.mDepthFullRatio =
        glm::vec2(depthPyramidExtent.width / static_cast<float>(depthExtent.width), depthPyramidExtent.height / static_cast<float>(depthExtent.height));

    // Work pass
    mCullResources.mWorkDescriptorSet.writeImage(
        0, mCullResources.mDepthPyramidImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mCullResources.mWorkDescriptorSet.pushWrites();

    mCullResources.mWorkPushConstants.mRenderInstancesCountBuffer = sRendererContext.mStats->mRenderInstancesCountBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneBoundsBuffer = mSceneBoundsBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mFrustumBuffer = mCamera.getFrustumBuffer().getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mSceneNodeTransformsBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneInstancesBuffer = mSceneInstancesBuffer.getDeviceAddress().value();
    mCullResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mSceneVisibleRenderInstancesInstanceIndexBuffer.getDeviceAddress().value();
    vk::Extent3D drawExtent = sRendererContext.mSwapchain->getDrawImage().getExtent();
    mCullResources.mWorkPushConstants.mDrawExtents = glm::vec2(drawExtent.width, drawExtent.height);
    mCullResources.mWorkPushConstants.mDepthPyramidExtents = glm::vec2(depthPyramidExtent.width, depthPyramidExtent.height);
}

void SwScene::initializePickResources() {
    mPickResources.mWorkBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        sizeof(SwPick::Data)
    );

    mPickResources.mDescriptorSetLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eCompute);
    mPickResources.mDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mPickResources.mDescriptorSetLayout);

    vk::PushConstantRange drawPushConstantRange = SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(SwPick::DrawPC));
    mPickResources.mDrawPipelineLayout = SwPipelineFactory::createPipelineLayout(nullptr, drawPushConstantRange);

    SwShader drawVertexShader = SwShaderFactory::createShader(PICK_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader drawFragmentShader = SwShaderFactory::createShader(PICK_DRAW_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    noBlendState.blendEnable = VK_FALSE;

    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getRawModule();
    drawPipelineOptions.mFragmentShader = drawFragmentShader.getRawModule();
    drawPipelineOptions.mLayout = mPickResources.mDrawPipelineLayout.getRawLayout();
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
    mPickResources.mDrawPipelinePipeline = SwGraphicsPipelineFactory::createGraphicsPipeline(drawPipelineOptions);

    vk::PushConstantRange pickPushConstantRange = SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(SwPick::WorkPC));
    mPickResources.mWorkPipelineLayout = SwPipelineFactory::createPipelineLayout({mPickResources.mDescriptorSetLayout.getRawLayout()}, pickPushConstantRange);

    SwShader workShader = SwShaderFactory::createShader(PICK_WORK_COMPUTE_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mPickResources.mWorkPipelinePipeline =
        SwComputePipelineFactory::createComputePipeline({workShader.getRawModule(), mPickResources.mWorkPipelineLayout.getRawLayout()});

    mPickResources.mDrawPushConstants.mSceneVertexBuffer = mSceneVertexBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mSceneNodeTransformsBuffer = mSceneNodeTransformsBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mSceneInstancesBuffer = mSceneInstancesBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mSceneVisibleRenderInstancesInstanceIndexBuffer.getDeviceAddress().value();
    mPickResources.mDrawPushConstants.mPostCullRenderItemsBuffer = 0;

    mPickResources.mWorkPushConstants.mPickerBuffer = mPickResources.mWorkBuffer.getDeviceAddress().value();

    onResizeInitializePickResources();
}

void SwScene::onResizeInitializePickResources() {
    vk::Extent3D drawExtent = sRendererContext.mSwapchain->getDrawImage().getExtent();
    mPickResources.mWorkImage = SwImageFactory::createColorImage2D(
        nullptr,
        vk::Format::eR32G32Uint,
        drawExtent,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        false
    );

    mPickResources.mDepthImage = SwImageFactory::createDepthImage2D(
        nullptr, SwSwapchain::DEPTH_FORMAT, mPickResources.mWorkImage.getExtent(), vk::ImageUsageFlagBits::eDepthStencilAttachment
    );

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mPickResources.mWorkImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );
        mPickResources.mDepthImage.emitTransition(
            cmd,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        );
    });

    mPickResources.mDescriptorSet.writeImage(
        0, mPickResources.mWorkImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mPickResources.mDescriptorSet.pushWrites();
}

void SwScene::changePickOperation() {
    switch (mPickResources.mImguizmoOperation) {
        case ImGuizmo::TRANSLATE:
            mPickResources.mImguizmoOperation = ImGuizmo::ROTATE;
            break;
        case ImGuizmo::ROTATE:
            mPickResources.mImguizmoOperation = ImGuizmo::SCALEU;
            break;
        case ImGuizmo::SCALEU:
            mPickResources.mImguizmoOperation = ImGuizmo::TRANSLATE;
            break;
        default:
            mPickResources.mImguizmoOperation = ImGuizmo::TRANSLATE;
    }
}

void SwScene::generatePickFrame() {
    if (mPickResources.mClickedInstance == nullptr) return;

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetGizmoSizeClipSpace(SwPick::PICK_IMGUIZMO_SIZE);

    ImGuizmo::SetRect(
        0,
        0,
        static_cast<float>(sRendererContext.mSwapchain->getDrawImage().getExtent().width),
        static_cast<float>(sRendererContext.mSwapchain->getDrawImage().getExtent().height)
    );

    mPickResources.mClickedInstance->getDataAddress()->mTransformMatrix;
    ImGuizmo::Manipulate(
        glm::value_ptr(mCamera.getPerspective().mView),
        glm::value_ptr(mCamera.getPerspective().mProj),
        mPickResources.mImguizmoOperation,
        ImGuizmo::WORLD,
        glm::value_ptr(mPickResources.mClickedInstance->getDataAddress()->mTransformMatrix)
    );

    if (ImGuizmo::IsUsing()) {
        mAssets[mPickResources.mClickedInstance->getAssetName()].setReloadInstancesFlag(true);
    }
}

void SwScene::initializeSkyboxResources() {
    mSkyboxResources.mWorkSampler = SwSamplerFactory::createSampler(vk::SamplerCreateInfo());

    mSkyboxResources.mWorkDescriptorLayout =
        sRendererContext.mDescriptorAllocator->createDescriptorLayout({{0, vk::DescriptorType::eCombinedImageSampler, 1}}, vk::ShaderStageFlagBits::eFragment);
    mSkyboxResources.mWorkDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mSkyboxResources.mWorkDescriptorLayout);

    vk::PushConstantRange skyboxPushConstantRange = SwPipelineFactory::createPushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(SwSkybox::WorkPC));
    mSkyboxResources.mWorkPipelineLayout =
        SwPipelineFactory::createPipelineLayout({mSkyboxResources.mWorkDescriptorLayout.getRawLayout()}, skyboxPushConstantRange);

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
    skyboxPipelineOptions.mLayout = mSkyboxResources.mWorkPipelineLayout.getRawLayout();
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
    mSkyboxResources.mWorkPipelinePipeline = SwGraphicsPipelineFactory::createGraphicsPipeline(skyboxPipelineOptions);

    const std::uint32_t skyboxVertexSize = static_cast<std::uint32_t>(mSkyboxResources.mWorkVertices.size() * sizeof(float));
    mSkyboxResources.mWorkVertexBuffer = SwBufferFactory::createAllocatedBuffer(
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        skyboxVertexSize
    );

    SwStagingBuffer skyboxVertexStagingBuffer = SwBufferFactory::createStagingBuffer(skyboxVertexSize);
    std::memcpy(skyboxVertexStagingBuffer.getMappedPointer(), mSkyboxResources.mWorkVertices.data(), skyboxVertexSize);

    vk::BufferCopy skyboxVertexCopy{};
    skyboxVertexCopy.dstOffset = 0;
    skyboxVertexCopy.srcOffset = 0;
    skyboxVertexCopy.size = skyboxVertexSize;

    sRendererContext.mImmSubmit->individualSubmit([&](vk::CommandBuffer cmd) {
        skyboxVertexStagingBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        mSkyboxResources.mWorkVertexBuffer.copyFrom(cmd, skyboxVertexStagingBuffer, skyboxVertexCopy, skyboxVertexCopy.size);
        mSkyboxResources.mWorkVertexBuffer.emitBarrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);
    });

    mSkyboxResources.mWorkPushConstants.mWorkVertexBuffer = mSkyboxResources.mWorkVertexBuffer.getDeviceAddress().value();

    onUpdateInitializeSkyboxResources();
}

void SwScene::onUpdateInitializeSkyboxResources() {
    if (!mSkyboxResources.mLoadFromDir.has_value()) {
        return;
    }

    std::vector<std::filesystem::path> skyboxImagePaths = {
        mSkyboxResources.mLoadFromDir.value() / "px.png",
        mSkyboxResources.mLoadFromDir.value() / "nx.png",
        mSkyboxResources.mLoadFromDir.value() / "py.png",
        mSkyboxResources.mLoadFromDir.value() / "ny.png",
        mSkyboxResources.mLoadFromDir.value() / "pz.png",
        mSkyboxResources.mLoadFromDir.value() / "nz.png"
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

    mSkyboxResources.mWorkImage = SwImageFactory::createColorImageCubemap(
        skyboxImageData.data(),
        vk::Format::eR8G8B8A8Srgb,
        vk::Extent3D{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        vk::ImageUsageFlagBits::eSampled,
        true
    );

    mSkyboxResources.mWorkDescriptorSet.writeImage(
        0,
        mSkyboxResources.mWorkImage.getRawMainImageView(),
        mSkyboxResources.mWorkSampler.getRawSampler(),
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::DescriptorType::eCombinedImageSampler
    );
    mSkyboxResources.mWorkDescriptorSet.pushWrites();
}

void SwScene::initializeWBOITResources() {
    mWBOITResources.mDescriptorSetLayout = sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        {{0, vk::DescriptorType::eSampledImage, 1}, {1, vk::DescriptorType::eSampledImage, 1}}, vk::ShaderStageFlagBits::eFragment
    );
    mWBOITResources.mDescriptorSet = sRendererContext.mDescriptorAllocator->createDescriptorSet(mWBOITResources.mDescriptorSetLayout);

    mWBOITResources.mPipelineLayout = SwPipelineFactory::createPipelineLayout({mWBOITResources.mDescriptorSetLayout.getRawLayout()}, nullptr);

    SwShader wboitVertexShader = SwShaderFactory::createShader(WBOIT_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    SwShader wboitFragmentShader = SwShaderFactory::createShader(WBOIT_FRAGMENT_SHADER_PATH, vk::ShaderStageFlagBits::eFragment);

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
    wboitPipelineOptions.mVertexShader = wboitVertexShader.getRawModule();
    wboitPipelineOptions.mFragmentShader = wboitFragmentShader.getRawModule();
    wboitPipelineOptions.mLayout = mWBOITResources.mPipelineLayout.getRawLayout();
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
    mWBOITResources.mPipelinePipeline = SwGraphicsPipelineFactory::createGraphicsPipeline(wboitPipelineOptions);

    onResizeInitializeWBOITResources();
}

void SwScene::onResizeInitializeWBOITResources() {
    vk::Extent3D drawExtent = sRendererContext.mSwapchain->getDrawImage().getExtent();
    mWBOITResources.mAccumImage = SwImageFactory::createColorImage2D(
        nullptr, SwSwapchain::DRAW_FORMAT, drawExtent, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, false
    );
    mWBOITResources.mRevealageImage = SwImageFactory::createColorImage2D(
        nullptr, vk::Format::eR16Sfloat, drawExtent, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, false
    );

    sRendererContext.mImmSubmit->addCallback([this](vk::CommandBuffer cmd) {
        mWBOITResources.mAccumImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead
        );
        mWBOITResources.mRevealageImage.emitTransition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead
        );
    });

    mWBOITResources.mDescriptorSet.writeImage(
        0, mWBOITResources.mAccumImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mWBOITResources.mDescriptorSet.writeImage(
        1, mWBOITResources.mRevealageImage.getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage
    );
    mWBOITResources.mDescriptorSet.pushWrites();
}

void SwScene::initializeGeometryResources() {
    mGeometryResources.mWorkPushConstants.mSceneVertexBuffer = mSceneVertexBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneMaterialConstantsBuffer = mSceneMaterialConstantsBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneNodeTransformsBuffer = mSceneNodeTransformsBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneInstancesBuffer = mSceneInstancesBuffer.getDeviceAddress().value();
    mGeometryResources.mWorkPushConstants.mSceneVisibleRenderInstancesInstanceIndexBuffer =
        mSceneVisibleRenderInstancesInstanceIndexBuffer.getDeviceAddress().value();
}

void SwScene::init(SwRendererContext rendererContext) { sRendererContext = rendererContext; }

void SwScene::initialize() {
    mCamera.initialize();
    initializeSceneResources();
    initializeCullResources();
    initializeGeometryResources();
    initializeSkyboxResources();
}

void SwScene::resize() {
    mCullResources.mDepthPyramidImage.destroy();
    onResizeInitializeCullResources();

    mPickResources.mWorkImage.destroy();
    mPickResources.mDepthImage.destroy();
    onResizeInitializePickResources();
}