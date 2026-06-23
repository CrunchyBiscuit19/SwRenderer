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

SwDescriptorLayout SwLighting::Resources::sShadowConsumeDescriptorLayout{};

void SwLighting::Resources::init() {
    sShadowConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "ShadowConsumeDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, NUM_LIGHT_CAST_SHADOWS},
            {1, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwLighting::Resources::cleanup() { sShadowConsumeDescriptorLayout.destroy(); }

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::selectActiveLights(
    const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount
) const {
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


glm::mat4 SwLighting::System::computeLightMatrix(const SwLight::Data& light, const glm::vec3& worldPos, const glm::vec3& worldDir) {
    const glm::vec3 forward = glm::normalize(worldDir);
    const glm::vec3 up = std::abs(forward.y) < 0.999f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);

    glm::mat4 view;
    glm::mat4 proj;
    if (light.mType == static_cast<std::uint32_t>(SwLight::Type::Spot)) {
        const float range = light.mRange > 0.f ? light.mRange : SwLighting::SHADOW_SPOT_DEFAULT_RANGE;
        const float halfAngle = std::acos(std::clamp(light.mOuterCos, -1.f, 1.f));
        const float fovy = std::clamp(2.f * halfAngle, glm::radians(1.f), glm::radians(179.f));
        view = glm::lookAt(worldPos, worldPos + forward, up);
        proj = glm::perspective(fovy, 1.f, range, SwLighting::SHADOW_SPOT_NEAR);
    } else {
        const glm::vec3 center{0.f};
        const glm::vec3 eye = center - forward * SwLighting::SHADOW_DIRECTIONAL_DISTANCE;
        view = glm::lookAt(eye, center, up);
        const float h = SwLighting::SHADOW_DIRECTIONAL_HALF_EXTENT;
        proj = glm::ortho(-h, h, -h, h, SwLighting::SHADOW_DIRECTIONAL_FAR, SwLighting::SHADOW_DIRECTIONAL_NEAR);
    }
    proj[1][1] *= -1.f;
    return proj * view;
}

void SwLighting::System::refreshActiveLights(const glm::vec3& cameraPos) {
    selectActiveLights(cameraPos, mResources.mActiveLightIndices, mResources.mActiveLightCount);

    mResources.mLightViewProj.fill(glm::mat4(1.f));
    for (std::uint32_t i = 0; i < mResources.mActiveLightCount; i++) {
        const std::uint32_t lightIndex = mResources.mActiveLightIndices[i];
        const SwLight::Data& light = mResources.mAssetLights[lightIndex];
        if (light.mType == static_cast<std::uint32_t>(SwLight::Type::Point)) {
            continue;
        }
        mResources.mLightViewProj[i] = computeLightMatrix(light, mResources.mLightWorldPositions[lightIndex], mResources.mLightWorldDirections[lightIndex]);
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
    // Comparison sampler: linear filtering gives hardware 2x2 PCF per SampleCmp tap, and eGreaterOrEqual matches the
    // reversed-Z stored depth (a receiver is lit when its depth is at least the nearest occluder's). The border is
    // opaque black (depth 0, the far plane) so taps that spill outside a spot frustum read as lit rather than shadowed.
    vk::SamplerCreateInfo shadowMapSamplerCreateInfo{};
    shadowMapSamplerCreateInfo.magFilter = vk::Filter::eLinear;
    shadowMapSamplerCreateInfo.minFilter = vk::Filter::eLinear;
    shadowMapSamplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
    shadowMapSamplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    shadowMapSamplerCreateInfo.minLod = 0.0f;
    shadowMapSamplerCreateInfo.maxLod = vk::LodClampNone;
    shadowMapSamplerCreateInfo.anisotropyEnable = vk::False;
    shadowMapSamplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    shadowMapSamplerCreateInfo.compareEnable = vk::True;
    shadowMapSamplerCreateInfo.compareOp = vk::CompareOp::eGreaterOrEqual;
    mResources.mShadowMapsSampler = SwSamplerFactory::createSampler("ShadowMapsSampler", shadowMapSamplerCreateInfo);

    mResources.mShadowMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("ShadowMapsDescriptorSet", Resources::sShadowConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        mResources.mShadowMapsDescriptorSet.writeImage(0, mResources.mShadowMaps[i].getRawMainImageView(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i);
    }
    mResources.mShadowMapsDescriptorSet.writeSampler(1, mResources.mShadowMapsSampler.getRawSampler());
    mResources.mShadowMapsDescriptorSet.pushWrites();

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
    
    drawPipelineOptions.mVertexEntryPoint = SHADOW_DRAW_OPAQUE_TRANSPARENT_ENTRY_POINT;
    mResources.mShadowDrawOpaqueTransparentPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);

    drawPipelineOptions.mVertexEntryPoint = SHADOW_DRAW_MASKED_ENTRY_POINT;
    mResources.mShadowDrawMaskedPipelineBundle = SwGraphicsPipelineFactory::createGraphicsPipeline("ShadowDrawPipeline", drawPipelineOptions);

    mResources.mShadowCullPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowCullPipelineLayout", nullptr, SwLighting::ShadowCullPC::getRange());
    SwShader cullShader = SwShaderFactory::createShader("ShadowCullShaderModule", SwLighting::SHADOW_CULL_SHADER_PATH, vk::ShaderStageFlagBits::eCompute);
    mResources.mShadowCullPipelineBundle =
        SwComputePipelineFactory::createComputePipeline("ShadowCullPipeline", {cullShader.getRawModule(), mResources.mShadowCullPipelineLayout.getRawLayout()});
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

    for (std::uint32_t i = 0; i < NUM_LIGHT_CAST_SHADOWS; i++) {
        staticDeps.mWriteImages.emplace_back(&mResources.mShadowMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
    }
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(
        SwPass::Type::LightingShadowDraw, std::move(staticDeps),
        [&](vk::CommandBuffer cmd) {
            const std::uint32_t count = std::min(mResources.mActiveLightCount, NUM_LIGHT_CAST_SHADOWS);
            auto& pipeline = mResources.mShadowDrawOpaqueTransparentPipelineBundle;

            mResources.mShadowDrawPc.mLightDrawRisIndicesBuffer = mScene.getSceneDrawRisIndicesBuffer().getDeviceAddress().value();

            for (std::uint32_t slot = 0; slot < count; slot++) {
                const std::uint32_t lightIndex = mResources.mActiveLightIndices[slot];
                // Point lights have no 2D shadow map yet (cube maps are a follow-up), so skip drawing into their slot.
                if (mResources.mAssetLights[lightIndex].mType == static_cast<std::uint32_t>(SwLight::Type::Point)) {
                    continue;
                }

                vk::RenderingAttachmentInfo depth = mResources.mShadowMaps[slot].generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
                cmd.beginRendering(SwPass::generateRenderingInfo(vk::Extent2D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT}, {}, depth));
                SwPass::setViewportScissors(cmd, vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1});

                cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getRawPipeline());
                cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getRawBuffer(), 0, vk::IndexType::eUint32);

                mResources.mShadowDrawPc.mLightIndex = slot;

                auto drawList = [&](SwBatch& batch, SwAllocatedBuffer& rcsBuffer, SwAllocatedBuffer& countBuffer) {
                    mResources.mShadowDrawPc.mLightRcsBuffer = rcsBuffer.getDeviceAddress().value();
                    cmd.pushConstants<SwLighting::ShadowDrawPC>(pipeline.getRawLayout(), SwLighting::ShadowDrawPC::sStages, 0, mResources.mShadowDrawPc);
                    cmd.drawIndexedIndirectCount(
                        rcsBuffer.getRawBuffer(), 0, countBuffer.getRawBuffer(), 0, static_cast<std::uint32_t>(batch.getRcs().size()), sizeof(SwRenderCommand)
                    );
                    SwRenderer::sRendererContext.mStats->mNumDrawCall++;
                };

                for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
                    if (batch.getRcs().empty()) {
                        continue;
                    }
                    drawList(batch, batch.getEarlyRcsBuffer(), batch.getEarlyRcsCount());
                    drawList(batch, batch.getFinalRcsBuffer(), batch.getFinalRcsCount());
                }
                for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Mask)) {
                    if (batch.getRcs().empty()) {
                        continue;
                    }
                    drawList(batch, batch.getFinalRcsBuffer(), batch.getFinalRcsCount());
                }

                cmd.endRendering();
            }
        },
        true
    );
    staticDeps.clear();
}

void SwLighting::System::refreshDynamicDependencies() {
    SwDependency dynamicDeps;
    dynamicDeps.mReadBuffers.emplace_back(
        &SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead
    );
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque)) {
        if (batch.getRcs().empty()) {
            continue;
        }
        dynamicDeps.mReadBuffers.emplace_back(&batch.getEarlyRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getEarlyRcsCount(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRcsCount(), SwDependency::BufferDepType::IndirectRead);
    }
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Mask)) {
        if (batch.getRcs().empty()) {
            continue;
        }
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRcsBuffer(), SwDependency::BufferDepType::IndirectRead);
        dynamicDeps.mReadBuffers.emplace_back(&batch.getFinalRcsCount(), SwDependency::BufferDepType::IndirectRead);
    }
    mScene.mPasses[SwPass::Type::LightingShadowDraw].setDynamicDeps(std::move(dynamicDeps));
}

void SwLighting::System::refreshPushConstants() {
    mResources.mShadowCullPc.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneBoundsBuffer = SwRenderer::sRendererContext.mScene->getSceneBoundsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowCullPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();

    mResources.mShadowDrawPc.mPerFrameBuffer = SwRenderer::sRendererContext.mSwapchain->getCurrentFrame().getPerFrameBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneVertexBuffer = SwRenderer::sRendererContext.mScene->getSceneVertexBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneNodeTransformsBuffer = SwRenderer::sRendererContext.mScene->getSceneNodeTransformsBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneInstancesBuffer = SwRenderer::sRendererContext.mScene->getSceneInstancesBuffer().getDeviceAddress().value();
    mResources.mShadowDrawPc.mSceneMaterialConstantsBuffer = SwRenderer::sRendererContext.mScene->getSceneMaterialConstantsBuffer().getDeviceAddress().value();
}