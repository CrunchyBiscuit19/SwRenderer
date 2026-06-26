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

SwDescriptorLayout SwLighting::Resources::sSpotShadowConsumeDescriptorLayout{};
SwDescriptorLayout SwLighting::Resources::sPointShadowConsumeDescriptorLayout{};
SwDescriptorLayout SwLighting::Resources::sDirShadowConsumeDescriptorLayout{};

void SwLighting::Resources::init() {
    sSpotShadowConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "SpotShadowConsumeDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, NUM_SPOT_SHADOWS},
            {1, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
    sPointShadowConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "PointShadowConsumeDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, NUM_POINT_SHADOWS},
            {1, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
    sDirShadowConsumeDescriptorLayout = SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorLayout(
        "DirShadowConsumeDescriptorLayout",
        {
            {0, vk::DescriptorType::eSampledImage, NUM_DIR_SHADOWS},
            {1, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwLighting::Resources::cleanup() {
    sSpotShadowConsumeDescriptorLayout.destroy();
    sPointShadowConsumeDescriptorLayout.destroy();
    sDirShadowConsumeDescriptorLayout.destroy();
}

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::spawnTestLight(SwLight::Type type, const glm::vec3& worldPos, const glm::vec3& worldDir) {
    SwLight light;
    switch (type) {
        case SwLight::Type::Directional:
            light = SwTestDirectionalLight();
            break;
        case SwLight::Type::Spot:
            light = SwTestSpotLight();
            break;
        case SwLight::Type::Point:
        default:
            light = SwTestPointLight();
            break;
    }
    light.getPosition() = worldPos;
    const float dirLength = glm::length(worldDir);
    light.getDirection() = dirLength > 1e-4f ? worldDir / dirLength : glm::vec3(0.f, 0.f, -1.f);
    mResources.mGlobalLights.emplace_back(light);
}

void SwLighting::System::selectActiveLights(
    const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount
) const {
    const std::vector<AssetLight>& assetLights = mResources.mAssetLights;
    const std::vector<SwLight>& globalLights = mResources.mGlobalLights;

    // Score every light by its perceived brightness at the camera, then keep the brightest MAX_ACTIVE_LIGHTS.
    std::vector<std::pair<float, std::uint32_t>> scored;
    scored.reserve(assetLights.size() + globalLights.size());

    auto scoreLight = [&](const SwLight::Params& params, const glm::vec3& worldPos, std::uint32_t index) {
        float score;
        if (params.mType == SwLight::Type::Directional) {
            score = std::numeric_limits<float>::max();  // no attenuation, always relevant
        } else {
            const glm::vec3 toLight = worldPos - cameraPos;
            const float dist2 = std::max(glm::dot(toLight, toLight), 1e-4f);
            float attenuation = 1.f / dist2;
            if (params.mRange > 0.f) {
                const float dist = std::sqrt(dist2);
                const float rangeFactor = std::clamp(1.f - std::pow(dist / params.mRange, 4.f), 0.f, 1.f);
                attenuation *= rangeFactor * rangeFactor;
            }
            const float luminance = glm::dot(params.mColor, glm::vec3(0.2126f, 0.7152f, 0.0722f));
            score = params.mIntensity * luminance * attenuation;
        }
        scored.emplace_back(score, index);
    };

    const std::uint32_t assetCount = static_cast<std::uint32_t>(assetLights.size());
    for (std::uint32_t i = 0; i < assetCount; i++) {
        scoreLight(assetLights[i].mLight->getParams(), assetLights[i].mWorldPosition, i);
    }
    for (std::uint32_t i = 0; i < globalLights.size(); i++) {
        scoreLight(globalLights[i].getParams(), globalLights[i].getPosition(), assetCount + i);
    }

    outCount = std::min<std::uint32_t>(static_cast<std::uint32_t>(scored.size()), SwLight::MAX_ACTIVE_LIGHTS);
    std::partial_sort(scored.begin(), scored.begin() + outCount, scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::uint32_t i = 0; i < outCount; i++) {
        outIndices[i] = scored[i].second;
    }
}

glm::mat4 SwLighting::System::computeLightMatrix(const SwLight::Params& params, const glm::vec3& worldPos, const glm::vec3& worldDir) {
    const glm::vec3 forward = glm::normalize(worldDir);
    const glm::vec3 up = std::abs(forward.y) < 0.999f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);

    glm::mat4 view;
    glm::mat4 proj;
    if (params.mType == SwLight::Type::Spot) {
        const float range = params.mRange > 0.f ? params.mRange : SwLighting::SHADOW_SPOT_DEFAULT_RANGE;
        const float fovy = std::clamp(2.f * params.mOuterConeAngle, glm::radians(1.f), glm::radians(179.f));
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

    const std::vector<AssetLight>& assetLights = mResources.mAssetLights;
    const std::vector<SwLight>& globalLights = mResources.mGlobalLights;

    mResources.mLightViewProj.fill(glm::mat4(1.f));
    mResources.mSpotShadowCount = 0;
    mResources.mPointShadowCount = 0;
    mResources.mDirShadowCount = 0;

    auto processLight = [&](std::uint32_t slot, const SwLight::Params& params, const glm::vec3& worldPos, const glm::vec3& worldDir) {
        const SwLight::Type type = params.mType;

        if (type == SwLight::Type::Spot) {
            if (mResources.mSpotShadowCount < NUM_SPOT_SHADOWS) {
                mResources.mSpotShadowLightIndices[mResources.mSpotShadowCount++] = slot;
            }
        } else if (type == SwLight::Type::Point) {
            if (mResources.mPointShadowCount < NUM_POINT_SHADOWS) {
                mResources.mPointShadowLightIndices[mResources.mPointShadowCount++] = slot;
            }
        } else if (type == SwLight::Type::Directional) {
            if (mResources.mDirShadowCount < NUM_DIR_SHADOWS) {
                mResources.mDirShadowLightIndices[mResources.mDirShadowCount++] = slot;
            }
        }

        // Point lights need a cube map (6 matrices)
        if (type == SwLight::Type::Point) {
            return;  // TODO 6 matrices
        }
        mResources.mLightViewProj[slot] = computeLightMatrix(params, worldPos, worldDir);
    };

    const std::uint32_t assetCount = static_cast<std::uint32_t>(assetLights.size());
    for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
        const std::uint32_t lightIndex = mResources.mActiveLightIndices[slot];
        if (lightIndex < assetCount) {
            const AssetLight& light = assetLights[lightIndex];
            processLight(slot, light.mLight->getParams(), light.mWorldPosition, light.mWorldDirection);
        } else {
            const SwLight& light = globalLights[lightIndex - assetCount];
            processLight(slot, light.getParams(), light.getPosition(), light.getDirection());
        }
    }
}

std::vector<SwLight::Data> SwLighting::System::collectLightData() const {
    std::vector<SwLight::Data> out;
    out.reserve(mResources.mAssetLights.size());
    for (const AssetLight& light : mResources.mAssetLights) {
        out.emplace_back(light.mLight->toData(light.mNodeTransformIndex, light.mInstanceIndex));
    }
    return out;
}

void SwLighting::System::initializeResources() {
    constexpr vk::ImageUsageFlags shadowMapUsage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

    // Linear filtering gives hardware 2x2 PCF per SampleCmp tap.
    // The 2D pools use opaque-black border so taps outside a frustum read as lit, while cube clamp to edge.
    auto makeComparisonSampler = [](const char* name, vk::SamplerAddressMode addressMode) {
        vk::SamplerCreateInfo info{};
        info.magFilter = vk::Filter::eLinear;
        info.minFilter = vk::Filter::eLinear;
        info.mipmapMode = vk::SamplerMipmapMode::eNearest;
        info.addressModeU = addressMode;
        info.addressModeV = addressMode;
        info.addressModeW = addressMode;
        info.minLod = 0.0f;
        info.maxLod = vk::LodClampNone;
        info.anisotropyEnable = vk::False;
        info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
        info.compareEnable = vk::True;
        info.compareOp = vk::CompareOp::eGreaterOrEqual;
        return SwSamplerFactory::createSampler(name, info);
    };

    // Spotlight 2D pool
    for (std::uint32_t i = 0; i < NUM_SPOT_SHADOWS; i++) {
        mResources.mSpotShadowMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("SpotShadowMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1},
            shadowMapUsage,
            true
        );
    }
    mResources.mSpotShadowMapsSampler = makeComparisonSampler("SpotShadowMapsSampler", vk::SamplerAddressMode::eClampToBorder);
    mResources.mSpotShadowMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("SpotShadowMapsDescriptorSet", Resources::sSpotShadowConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < NUM_SPOT_SHADOWS; i++) {
        mResources.mSpotShadowMapsDescriptorSet.writeImage(
            0, mResources.mSpotShadowMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mSpotShadowMapsDescriptorSet.writeSampler(1, mResources.mSpotShadowMapsSampler.getHandle());
    mResources.mSpotShadowMapsDescriptorSet.pushWrites();

    // Point-light cube pool
    for (std::uint32_t i = 0; i < NUM_POINT_SHADOWS; i++) {
        mResources.mPointShadowMaps[i] = SwImageFactory::createDepthImageCubemap(
            std::format("PointShadowMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_CUBE_MAP_WIDTH_HEIGHT, SHADOW_CUBE_MAP_WIDTH_HEIGHT, 1},
            shadowMapUsage,
            false
        );
    }
    mResources.mPointShadowMapsSampler = makeComparisonSampler("PointShadowMapsSampler", vk::SamplerAddressMode::eClampToEdge);
    mResources.mPointShadowMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("PointShadowMapsDescriptorSet", Resources::sPointShadowConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < NUM_POINT_SHADOWS; i++) {
        mResources.mPointShadowMapsDescriptorSet.writeImage(
            0, mResources.mPointShadowMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mPointShadowMapsDescriptorSet.writeSampler(1, mResources.mPointShadowMapsSampler.getHandle());
    mResources.mPointShadowMapsDescriptorSet.pushWrites();

    // Directional 2D pool
    for (std::uint32_t i = 0; i < NUM_DIR_SHADOWS; i++) {
        mResources.mDirShadowMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("DirShadowMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1},
            shadowMapUsage,
            true
        );
    }
    mResources.mDirShadowMapsSampler = makeComparisonSampler("DirShadowMapsSampler", vk::SamplerAddressMode::eClampToBorder);
    mResources.mDirShadowMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("DirShadowMapsDescriptorSet", Resources::sDirShadowConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < NUM_DIR_SHADOWS; i++) {
        mResources.mDirShadowMapsDescriptorSet.writeImage(
            0, mResources.mDirShadowMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mDirShadowMapsDescriptorSet.writeSampler(1, mResources.mDirShadowMapsSampler.getHandle());
    mResources.mDirShadowMapsDescriptorSet.pushWrites();

    for (std::uint32_t i = 0; i < NUM_SPOT_SHADOWS; i++) {
        mResources.mSpotLightDrawRisIndicesBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("SpotLightDrawRisIndicesBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            SwScene::SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t),
            true
        );
        mResources.mSpotLightRcsBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            "SpotLightRcsBuffer", vk::BufferUsageFlagBits::eStorageBuffer, SwBatch::RENDER_COMMANDS_INITIAL_BUFFER_SIZE, true
        );
    }

    mResources.mShadowDrawPipelineLayout = SwPipelineFactory::createPipelineLayout("ShadowDrawPipelineLayout", nullptr, SwLighting::ShadowDrawPC::getRange());
    SwShader drawVertexShader =
        SwShaderFactory::createShader("ShadowDrawVertexShaderModule", SwLighting::SHADOW_DRAW_VERTEX_SHADER_PATH, vk::ShaderStageFlagBits::eVertex);
    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.blendEnable = VK_FALSE;
    SwGraphicsPipelineFactory::SwGraphicsPipelineOptions drawPipelineOptions;
    drawPipelineOptions.mVertexShader = drawVertexShader.getHandle();
    drawPipelineOptions.mFragmentShader = std::nullopt;
    drawPipelineOptions.mLayout = mResources.mShadowDrawPipelineLayout.getHandle();
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
        SwComputePipelineFactory::createComputePipeline("ShadowCullPipeline", {cullShader.getHandle(), mResources.mShadowCullPipelineLayout.getHandle()});
}

void SwLighting::System::initializePasses() {
    SwDependency staticDeps;

    for (std::uint32_t i = 0; i < NUM_SPOT_SHADOWS; i++) {
        staticDeps.mWriteBuffers.emplace_back(&mResources.mSpotLightDrawRisIndicesBuffer[i], SwDependency::BufferDepType::TransferWrite);
        staticDeps.mWriteBuffers.emplace_back(&mResources.mSpotLightRcsBuffer[i], SwDependency::BufferDepType::TransferWrite);
    }
    mScene.insertPass(SwPass::Type::LightingShadowReset, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < NUM_SPOT_SHADOWS; i++) {
            cmd.fillBuffer(mResources.mSpotLightDrawRisIndicesBuffer[i].getHandle(), 0, VK_WHOLE_SIZE, 0);
            cmd.fillBuffer(mResources.mSpotLightRcsBuffer[i].getHandle(), 0, VK_WHOLE_SIZE, 0);
        }
    });
    staticDeps.clear();

    mScene.insertPass(SwPass::Type::LightingShadowCull, std::move(staticDeps), [&](vk::CommandBuffer cmd) {

    });
    staticDeps.clear();

    for (std::uint32_t i = 0; i < NUM_SPOT_SHADOWS; i++) {
        staticDeps.mWriteImages.emplace_back(&mResources.mSpotShadowMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
    }
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneDrawRisIndicesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(
        SwPass::Type::LightingShadowDraw,
        std::move(staticDeps),
        [&](vk::CommandBuffer cmd) {
            auto& pipeline = mResources.mShadowDrawOpaqueTransparentPipelineBundle;

            mResources.mShadowDrawPc.mLightDrawRisIndicesBuffer = mScene.getSceneDrawRisIndicesBuffer().getDeviceAddress().value();

            // The spot selection list holds active-light slots, each of which is also its spot-map slot.
            for (std::uint32_t s = 0; s < mResources.mSpotShadowCount; s++) {
                const std::uint32_t slot = mResources.mSpotShadowLightIndices[s];

                vk::RenderingAttachmentInfo depth = mResources.mSpotShadowMaps[slot].generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
                cmd.beginRendering(SwPass::generateRenderingInfo(vk::Extent2D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT}, {}, depth));
                SwPass::setViewportScissors(cmd, vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1});

                cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
                cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);

                mResources.mShadowDrawPc.mLightIndex = slot;

                auto drawList = [&](SwBatch& batch, SwAllocatedBuffer& rcsBuffer, SwAllocatedBuffer& countBuffer) {
                    mResources.mShadowDrawPc.mLightRcsBuffer = rcsBuffer.getDeviceAddress().value();
                    cmd.pushConstants<SwLighting::ShadowDrawPC>(pipeline.getLayouthandle(), SwLighting::ShadowDrawPC::sStages, 0, mResources.mShadowDrawPc);
                    cmd.drawIndexedIndirectCount(
                        rcsBuffer.getHandle(), 0, countBuffer.getHandle(), 0, static_cast<std::uint32_t>(batch.getRcs().size()), sizeof(SwRenderCommand)
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