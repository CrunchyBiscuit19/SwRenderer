#include <Renderer/SwHelper.h>
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
            {0, vk::DescriptorType::eSampledImage, NUM_2D_SHADOWS},
            {1, vk::DescriptorType::eSampledImage, NUM_CUBE_SHADOWS},
            {2, vk::DescriptorType::eSampler, 1},
        },
        vk::ShaderStageFlagBits::eFragment
    );
}

void SwLighting::Resources::cleanup() { sShadowConsumeDescriptorLayout.destroy(); }

SwLighting::System::System(SwScene& scene) : SwSystem(scene) {}

void SwLighting::System::resolveLightWorld(const SwLight& light, glm::vec3& outPosition, glm::vec3& outDirection) {
    const glm::mat4 instanceTransform = mScene.getInstance(light.getInstanceId()).getData().mTransformMatrix;
    const glm::mat4 nodeWorldTransform = mScene.getAsset(light.getAssetId()).getNodes()[light.getRelativeNodeIndex()]->getWorldTransform();
    outPosition = light.worldPosition(instanceTransform, nodeWorldTransform);
    outDirection = light.worldDirection(instanceTransform, nodeWorldTransform);
}

void SwLighting::System::selectActiveLights(
    const glm::vec3& cameraPos, std::array<std::uint32_t, SwLight::MAX_ACTIVE_LIGHTS>& outIndices, std::uint32_t& outCount
) {
    const std::vector<std::uint32_t>& lightIds = mScene.getLightIds();

    // Score every light by its perceived brightness at the camera, then keep the brightest MAX_ACTIVE_LIGHTS.
    std::vector<std::pair<float, std::uint32_t>> scored;
    scored.reserve(lightIds.size());

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

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(lightIds.size()); i++) {
        const SwLight& light = mScene.getLight(lightIds[i]);
        glm::vec3 worldPos, worldDir;
        resolveLightWorld(light, worldPos, worldDir);
        scoreLight(light.getParams(), worldPos, i);
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

    const std::vector<std::uint32_t>& lightIds = mScene.getLightIds();

    // The per-light view-projection is no longer stored on the resources. It lives only long enough to seed
    // the shadow frustums, so it is rebuilt into this frame-local array each time.
    std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS> lightViewProj;
    lightViewProj.fill(glm::mat4(1.f));
    mResources.mShadowType.fill(ShadowType::None);
    mResources.mShadowIndex.fill(0);

    std::uint32_t next2D = 0;
    std::uint32_t nextCube = 0;

    auto processLight = [&](std::uint32_t slot, const SwLight::Params& params, const glm::vec3& worldPos, const glm::vec3& worldDir) {
        const SwLight::Type type = params.mType;

        if (type == SwLight::Type::Point) {
            if (nextCube < NUM_CUBE_SHADOWS) {
                mResources.mShadowType[slot] = ShadowType::Cube;
                mResources.mShadowIndex[slot] = nextCube++;
                // Point lights need a cube map (6 matrices). TODO: render the cube faces.
            }
            return;
        }

        if (next2D < NUM_2D_SHADOWS) {
            mResources.mShadowType[slot] = ShadowType::TwoD;
            mResources.mShadowIndex[slot] = next2D++;
            lightViewProj[slot] = computeLightMatrix(params, worldPos, worldDir);
        }
    };

    for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
        const std::uint32_t lightIndex = mResources.mActiveLightIndices[slot];
        const SwLight& light = mScene.getLight(lightIds[lightIndex]);
        glm::vec3 worldPos, worldDir;
        resolveLightWorld(light, worldPos, worldDir);
        processLight(slot, light.getParams(), worldPos, worldDir);
    }

    prepareShadowCullData(lightViewProj);

    ActiveLights packed{};
    packed.mCount = mResources.mActiveLightCount;
    packed.mIndices = mResources.mActiveLightIndices;
    packed.mShadowType = mResources.mShadowType;
    packed.mShadowIndex = mResources.mShadowIndex;
    mResources.mActiveLightsBuffer.copyFromUnchecked(&packed, sizeof(ActiveLights));
}

void SwLighting::System::calculateFrustum(const glm::mat4& m, SwCull::Plane* out) {
    const glm::vec4 row0{m[0][0], m[1][0], m[2][0], m[3][0]};
    const glm::vec4 row1{m[0][1], m[1][1], m[2][1], m[3][1]};
    const glm::vec4 row2{m[0][2], m[1][2], m[2][2], m[3][2]};
    const glm::vec4 row3{m[0][3], m[1][3], m[2][3], m[3][3]};

    const glm::vec4 planes[SwLighting::NUM_FRUSTUM_PLANES]{
        row3 + row0,  // left
        row3 - row0,  // right
        row3 + row1,  // bottom
        row3 - row1,  // top
        row2,         // near
        row3 - row2,  // far
    };

    for (std::uint32_t i = 0; i < SwLighting::NUM_FRUSTUM_PLANES; i++) {
        const glm::vec3 normal{planes[i]};
        const float length = glm::length(normal);
        const float invLength = length > 0.f ? 1.f / length : 0.f;
        out[i].mNormal = normal * invLength;
        out[i].mDistance = -planes[i].w * invLength;
    }
}

void SwLighting::System::prepareShadowCullData(const std::array<glm::mat4, SwLight::MAX_ACTIVE_LIGHTS>& lightViewProj) {
    mResources.mShadowRcs.clear();

    mResources.mShadowCullPc.mShadowRisLimit = 0;
    for (auto& batch : mScene.getBatchIt(SwMaterial::Type::Opaque, SwMaterial::Type::Mask)) {
        for (SwRenderCommand rc : batch.getRcs()) {
            rc.mRiCount = 0;
            mResources.mShadowRcs.emplace_back(rc);
        }
        mResources.mShadowCullPc.mShadowRisLimit += batch.getRis().size();
    }

    mResources.mShadowFrustums.fill(SwCull::Plane{});
    for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
        if (mResources.mShadowType[slot] != ShadowType::TwoD) continue;  // Skip point
        calculateFrustum(lightViewProj[slot], &mResources.mShadowFrustums[mResources.mShadowIndex[slot] * NUM_FRUSTUM_PLANES]);
    }
}

void SwLighting::System::initializeResources() {
    mResources.mActiveLightsBuffer = SwBufferFactory::createAllocatedBuffer(
        "ActiveLightsBuffer",
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        sizeof(ActiveLights),
        true
    );

    // Linear filtering gives hardware 2x2 PCF per SampleCmp tap.
    // Opaque-black border so 2D taps outside a frustum read as lit; harmless for seamless cube sampling.
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

    for (std::uint32_t i = 0; i < NUM_2D_SHADOWS; i++) {
        mResources.mShadow2DMaps[i] = SwImageFactory::createDepthImage2D(
            std::format("Shadow2DMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            true
        );
    }
    for (std::uint32_t i = 0; i < NUM_CUBE_SHADOWS; i++) {
        mResources.mShadowCubeMaps[i] = SwImageFactory::createDepthImageCubemap(
            std::format("ShadowCubeMap{}", i),
            nullptr,
            SHADOW_MAP_FORMAT,
            vk::Extent3D{SHADOW_CUBE_MAP_WIDTH_HEIGHT, SHADOW_CUBE_MAP_WIDTH_HEIGHT, 1},
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            false
        );
    }

    mResources.mShadowMapsSampler = makeComparisonSampler("ShadowMapsSampler", vk::SamplerAddressMode::eClampToBorder);
    mResources.mShadowMapsDescriptorSet =
        SwRenderer::sRendererContext.mDescriptorAllocator->createDescriptorSet("ShadowMapsDescriptorSet", Resources::sShadowConsumeDescriptorLayout);
    for (std::uint32_t i = 0; i < NUM_2D_SHADOWS; i++) {
        mResources.mShadowMapsDescriptorSet.writeImage(
            0, mResources.mShadow2DMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    for (std::uint32_t i = 0; i < NUM_CUBE_SHADOWS; i++) {
        mResources.mShadowMapsDescriptorSet.writeImage(
            1, mResources.mShadowCubeMaps[i].getMainImageViewHandle(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, i
        );
    }
    mResources.mShadowMapsDescriptorSet.writeSampler(2, mResources.mShadowMapsSampler.getHandle());
    mResources.mShadowMapsDescriptorSet.pushWrites();

    for (std::uint32_t i = 0; i < NUM_2D_SHADOWS; i++) {
        mResources.mShadowRisIndicesBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightDrawRisIndicesBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            SwScene::SCENE_INITIAL_NUM_RENDER_ITEMS * sizeof(std::uint32_t),
            true
        );
        mResources.mShadowRcsBuffer[i] = SwBufferFactory::createAllocatedBuffer(
            std::format("Shadow2DLightRcsBuffer{}", i),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,  // Will change frame to frame
            SHADOW_RCS_BUFFER_SIZE,
            true
        );
    }

    mResources.mShadowFrustumsBuffer = SwBufferFactory::createAllocatedBuffer(
        "ShadowFrustumBuffer",
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        NUM_2D_SHADOWS * NUM_FRUSTUM_PLANES * sizeof(SwCull::Plane),
        true
    );

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

    for (std::uint32_t i = 0; i < NUM_2D_SHADOWS; i++) {
        staticDeps.mWriteBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::HostWrite);
    }
    staticDeps.mWriteBuffers.emplace_back(&mResources.mShadowFrustumsBuffer, SwDependency::BufferDepType::HostWrite);
    mScene.insertPass(SwPass::Type::LightingShadowReset, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        for (std::uint32_t i = 0; i < mResources.mActiveLightCount; i++) {
            mResources.mShadowRcsBuffer[mResources.mShadowIndex[i]].copyFromUnchecked(
                mResources.mShadowRcs.data(), mResources.mShadowRcs.size() * sizeof(SwRenderCommand)
            );
        }
        mResources.mShadowFrustumsBuffer.copyFromUnchecked(mResources.mShadowFrustums.data(), mResources.mShadowFrustums.size() * sizeof(SwCull::Plane));
    });
    staticDeps.clear();

    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneBoundsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::ComputeStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mResources.mShadowFrustumsBuffer, SwDependency::BufferDepType::ComputeStorageRead);
    for (std::uint32_t i = 0; i < NUM_2D_SHADOWS; i++) {
        staticDeps.mWriteBuffers.emplace_back(
            &mResources.mShadowRcsBuffer[i],
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
        );
        staticDeps.mWriteBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::ComputeStorageWrite);
    }
    mScene.insertPass(SwPass::Type::LightingShadowCull, std::move(staticDeps), [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(mResources.mShadowCullPipelineBundle.getBindPoint(), mResources.mShadowCullPipelineBundle.getPipelineHandle());
        for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
            
        }
    });
    staticDeps.clear();

    for (std::uint32_t i = 0; i < NUM_2D_SHADOWS; i++) {
        staticDeps.mWriteImages.emplace_back(&mResources.mShadow2DMaps[i], SwDependency::ImageDepType::DepthAttachmentReadWrite);
        staticDeps.mReadBuffers.emplace_back(&mResources.mShadowRcsBuffer[i], SwDependency::BufferDepType::IndirectRead);
        staticDeps.mReadBuffers.emplace_back(&mResources.mShadowRisIndicesBuffer[i], SwDependency::BufferDepType::VertexShaderStorageRead);
    }
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneVertexBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneNodeTransformsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneInstancesBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneMaterialConstantsBuffer(), SwDependency::BufferDepType::VertexShaderStorageRead);
    staticDeps.mReadBuffers.emplace_back(&mScene.getSceneIndexBuffer(), SwDependency::BufferDepType::IndexRead);
    mScene.insertPass(SwPass::Type::LightingShadowDraw, std::move(staticDeps), [&](vk::CommandBuffer cmd) {}, true);
    staticDeps.clear();
}

void SwLighting::System::refreshDynamicDependencies() {}

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

/*auto& pipeline = mResources.mShadowCullPipelineBundle;
cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());

const std::vector<AssetLight>& assetLights = mResources.mAssetLights;
for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
    if (mResources.mShadowType[slot] != ShadowType::TwoD) {
        continue;
    }
    const std::uint32_t mapIndex = mResources.mShadowIndex[slot];
    const SwLight::Params& params = assetLights[mResources.mActiveLightIndices[slot]].mLight->getParams();

    mResources.mShadowCullPc.mLightWorldPos = assetLights[mResources.mActiveLightIndices[slot]].mWorldPosition;
    mResources.mShadowCullPc.mLightRange = params.mRange;
    mResources.mShadowCullPc.mLightType = static_cast<std::uint32_t>(params.mType);
    mResources.mShadowCullPc.mFrustumBuffer =
        mResources.mShadowFrustumBuffer.getDeviceAddress().value() + mapIndex * NUM_FRUSTUM_PLANES * sizeof(SwCull::Plane);
    mResources.mShadowCullPc.mLightDrawRisIndicesBuffer = mResources.mShadowRisIndicesBuffer[mapIndex].getDeviceAddress().value();

    const vk::DeviceAddress rcsBase = mResources.mShadowRcsBuffer[mapIndex].getDeviceAddress().value();
    for (const ShadowBatch& shadowBatch : mResources.mShadowBatches) {
        const std::uint32_t risCount = static_cast<std::uint32_t>(shadowBatch.mBatch->getRis().size());
        if (risCount == 0) {
            continue;
        }
        mResources.mShadowCullPc.mLightRcsBuffer = rcsBase + shadowBatch.mRcsByteOffset;
        mResources.mShadowCullPc.mLightRisBuffer = shadowBatch.mBatch->getRisBuffer().getDeviceAddress().value();
        mResources.mShadowCullPc.mLightRisCount = risCount;
        cmd.pushConstants<SwLighting::ShadowCullPC>(pipeline.getLayoutHandle(), SwLighting::ShadowCullPC::sStages, 0, mResources.mShadowCullPc);
        cmd.dispatch(SwHelper::fastDivCeil(risCount, SwRenderer::MAX_1D_WORKGROUP_THREADS), 1, 1);
    }
}*/

/*for (std::uint32_t slot = 0; slot < mResources.mActiveLightCount; slot++) {
if (mResources.mShadowType[slot] != ShadowType::TwoD) {
continue;
}
const std::uint32_t mapIndex = mResources.mShadowIndex[slot];

vk::RenderingAttachmentInfo depth = mResources.mShadow2DMaps[mapIndex].generateRenderingAttachment(vk::AttachmentLoadOp::eClear);
cmd.beginRendering(SwPass::generateRenderingInfo(vk::Extent2D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT}, {}, depth));
SwPass::setViewportScissors(cmd, vk::Extent3D{SHADOW_MAP_WIDTH_HEIGHT, SHADOW_MAP_WIDTH_HEIGHT, 1});
cmd.bindIndexBuffer(mScene.getSceneIndexBuffer().getHandle(), 0, vk::IndexType::eUint32);

mResources.mShadowDrawPc.mLightIndex = slot;
mResources.mShadowDrawPc.mLightDrawRisIndicesBuffer = mResources.mShadowDrawRisIndicesBuffer[mapIndex].getDeviceAddress().value();
const vk::DeviceAddress rcsBase = mResources.mShadowRcsBuffer[mapIndex].getDeviceAddress().value();

SwGraphicsPipelineBundle* bound = nullptr;
for (const ShadowBatch& shadowBatch : mResources.mShadowBatches) {
SwGraphicsPipelineBundle& pipeline =
shadowBatch.mMasked ? mResources.mShadowDrawMaskedPipelineBundle : mResources.mShadowDrawOpaqueTransparentPipelineBundle;
if (bound != &pipeline) {
cmd.bindPipeline(pipeline.getBindPoint(), pipeline.getPipelineHandle());
bound = &pipeline;
}

mResources.mShadowDrawPc.mLightRcsBuffer = rcsBase + shadowBatch.mRcsByteOffset;
cmd.pushConstants<SwLighting::ShadowDrawPC>(pipeline.getLayoutHandle(), SwLighting::ShadowDrawPC::sStages, 0, mResources.mShadowDrawPc);
cmd.drawIndexedIndirect(
mResources.mShadowRcsBuffer[mapIndex].getHandle(), shadowBatch.mRcsByteOffset, shadowBatch.mRcsCount, sizeof(SwRenderCommand)
);
SwRenderer::sRendererContext.mStats->mNumDrawCall++;
}

cmd.endRendering();
}*/