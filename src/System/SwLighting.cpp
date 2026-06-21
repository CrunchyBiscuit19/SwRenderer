#include <Renderer/SwRenderer.h>
#include <Resource/SwSampler.h>
#include <Resource/SwShader.h>
#include <Scene/SwScene.h>
#include <System/SwLighting.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::selectActiveLights(const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount) const {
    const std::vector<SwLight::Data>& lights = mResources.mAssetLights;
    const std::vector<glm::vec3>& positions = mResources.mLightWorldPositions;

    // Score every light by its perceived brightness at the camera, then keep the brightest MAX_ACTIVE_LIGHTS.
    std::vector<std::pair<float, std::uint32_t>> scored;
    scored.reserve(lights.size());
    for (std::uint32_t i = 0; i < lights.size(); i++) {
        const SwLight::Data& light = lights[i];

        float score;
        if (light.mType == static_cast<std::uint32_t>(SwLight::Type::Directional)) {
            score = std::numeric_limits<float>::max();  // no attenuation, always relevant
        } else {
            const glm::vec3 toLight = positions[i] - cameraPos;
            const float dist2 = std::max(glm::dot(toLight, toLight), 1e-4f);
            float attenuation = 1.f / dist2;  
            if (light.mRange > 0.f) {
                const float dist = std::sqrt(dist2);
                const float rangeFactor = std::clamp(1.f - std::pow(dist / light.mRange, 4.f), 0.f, 1.f);
                attenuation *= rangeFactor * rangeFactor;
            }
            const float luminance = glm::dot(light.mColor, glm::vec3(0.2126f, 0.7152f, 0.0722f));
            score = light.mIntensity * luminance * attenuation;
        }
        scored.emplace_back(score, i);
    }

    outCount = std::min<std::uint32_t>(static_cast<std::uint32_t>(scored.size()), SwLight::MAX_ACTIVE_LIGHTS);
    std::partial_sort(scored.begin(), scored.begin() + outCount, scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::uint32_t i = 0; i < outCount; i++) {
        outIndices[i] = scored[i].second;
    }
}

void SwLighting::System::initializeResources() {
    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        mResources.mShadowMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("ShadowMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    vk::SamplerCreateInfo shadowMapSamplerCreateInfo{};
    shadowMapSamplerCreateInfo.magFilter = vk::Filter::eNearest;
    shadowMapSamplerCreateInfo.minFilter = vk::Filter::eNearest;
    shadowMapSamplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    shadowMapSamplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.minLod = 0.0f;
    shadowMapSamplerCreateInfo.maxLod = vk::LodClampNone;
    shadowMapSamplerCreateInfo.anisotropyEnable = vk::True;
    shadowMapSamplerCreateInfo.maxAnisotropy = SwSamplerFactory::getMaxSamplerAnisotropy();
    shadowMapSamplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    shadowMapSamplerCreateInfo.compareEnable = vk::False;
    mResources.mShadowMapsSampler = SwSamplerFactory::createSampler("ShadowMapsSampler", shadowMapSamplerCreateInfo);

    mResources.mShadowMapsDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "ShadowMapsDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, NUM_LIGHT_CAST_SHADOWS},
            {1, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment 
    );
    mResources.mShadowMapsDescriptorSet = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet(
        "ShadowMapsDescriptorSet", mResources.mShadowMapsDescriptorLayout, NUM_LIGHT_CAST_SHADOWS
    );

    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        mResources.mLightDrawRisIndicesBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("LightDrawRisIndicesBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            SwScene::SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t),
            true
        );
        mResources.mLightRcsBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            "LightRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, SwBatch::RENDER_COMMANDS_INITIAL_BUFFER_SIZE, true
        );
    }

    mResources.mShadowDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowDrawPipelineLayout", nullptr, SwLighting::ShadowDrawPC::getRange());
    SwShader drawVertexShader =
        SwShaderFactory::createShader("ShadowDrawVertexShaderModule", SwLighting::SHADOW_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.blendEnable = VK_FALSE;
    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getRawModule();
    drawPipelineOptions.mFragmentShader = std::nullopt;
    drawPipelineOptions.mLayout = mResources.mShadowDrawPipelineLayout.getRawLayout();
    drawPipelineOptions.mTopology = vk::PrimitiveTopology::eTriangleList;
    drawPipelineOptions.mPolygonMode = vk::PolygonMode::eFill;
    drawPipelineOptions.mCullMode = vk::CullModeFlagBits::eFront;
    drawPipelineOptions.mFrontFace = vk::FrontFace::eCounterClockwise;
    drawPipelineOptions.mMultisamplingEnabled = false;
    drawPipelineOptions.mSampleShadingEnabled = false;
    drawPipelineOptions.mColorAttachments = {};
    drawPipelineOptions.mDepthFormat = SHADOW_MAP_FORMAT;
    drawPipelineOptions.mDepthTestEnabled = true;
    drawPipelineOptions.mDepthWriteEnabled = true;
    drawPipelineOptions.mDepthCompareOp = vk::CompareOp::eGreaterOrEqual;
    drawPipelineOptions.mVertexEntryPoint = SHADOW_DRAW_OPAQUE_ENTRY_POINT;
    mResources.mShadowDrawPipelineBundle =
        SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);

    mResources.mShadowCullPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowCullPipelineLayout", nullptr, SwLighting::ShadowCullPC::getRange());
    SwShader cullShader =
        SwShaderFactory::createShader("ShadowCullShaderModule", SwLighting::SHADOW_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowCullPipelineBundle = SwComputePipelineFactory::createComputePipeline(
        "ShadowCullPipeline", {cullShader.getRawModule(), mResources.mShadowCullPipelineLayout.getRawLayout()}
    );
}

void SwLighting::System::initializePasses() {
    SwDependency staticDeps;

    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        staticDeps.mWriteBuffers.emplace_back(&mResources.mLightDrawRisIndicesBuffer[i], SwDependency::BufferDepType::TransferWrite);
        staticDeps.mWriteBuffers.emplace_back(&mResources.mLightRcsBuffer[i], SwDependency::BufferDepType::TransferWrite);
    }
    mScene.insertPass(SwPass::Type::LightingShadowReset, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
            cmd.fillBuffer(mResources.mLightDrawRisIndicesBuffer[i].getRawBuffer(), 0, VK_WHOLE_SIZE, 0);
            cmd.fillBuffer(mResources.mLightRcsBuffer[i].getRawBuffer(), 0, VK_WHOLE_SIZE, 0);
        }
    });
    staticDeps.clear();

    mScene.insertPass(SwPass::Type::LightingShadowCull, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
            
    });
    staticDeps.clear();

    mScene.insertPass(SwPass::Type::LightingShadowDraw, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
            
    });
    staticDeps.clear();
}

void SwLighting::System::refreshDynamicDependencies() {}

void SwLighting::System::refreshPushConstants() {
    mResources.mShadowCullPc.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneBoundsBuffer = SwRenderer::sRendererContext.mScene->getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneNodeTransformsBuffer =
        SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();

    mResources.mShadowDrawPc.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneVertexBuffer = SwRenderer::sRendererContext.mScene->getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneMaterialConstantsBuffer = SwRenderer::sRendererContext.mScene->getSceneMaterialConstantsBuffer().getDeviceAddress().value();
}