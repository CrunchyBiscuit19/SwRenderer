import System.Cull.SwCull;
import Data.SwConstants;
import Data.SwMaterial;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthPyramidImage;

[[vk::binding(1, 0)]]
public uniform SamplerState minSampler;

[[vk::push_constant]]
SwCull::TestPC testPc;

void transformCornersToWorld(inout float4[SwMesh::NUM_AABB_CORNERS] aabbCorners, SwInstance::Data* currInstance, SwNode::Transform* nodeTransform) {
    for (uint j = 0; j < SwMesh::NUM_AABB_CORNERS; j++) {
        aabbCorners[j] = mul(currInstance->mTransformMatrix, mul(*nodeTransform, aabbCorners[j]));
    }
}

bool occlusionCull(inout float4[SwMesh::NUM_AABB_CORNERS] aabbCorners, SwCamera::Perspective* perspective, Texture2D<float> depthPyramidImage) {
    float4 aabbMinNdc = float4(1.f, 1.f, 1.f, 0.f);
    float4 aabbMaxNdc = float4(-1.f, -1.f, 0.f, 0.f);
    float closestDepthInRi = 0.f;

    // Transform AABB corners to NDC / Screen space and find min/max XY and closest Z
    // Vulkan NDC is [0, 1] for Z and [-1, 1] for XY
    // Reverse-Z in effect (1 == near, 0 == far)
    for (uint j = 0; j < SwMesh::NUM_AABB_CORNERS; j++) {
        float4 clipPos = mul(perspective->mProj, mul(perspective->mView, aabbCorners[j]));

        // Corner is behind the near plane: w <= 0 means perspective division would flip
        // coordinates. Treat the whole object as visible (conservative fallback).
        if (clipPos.w <= 0.f)
            return true;

        float4 ndc = clipPos / clipPos.w;
        aabbCorners[j] = ndc;
        aabbMinNdc = min(aabbMinNdc, ndc);
        aabbMaxNdc = max(aabbMaxNdc, ndc);

        closestDepthInRi = max(closestDepthInRi, ndc.z);
    }

    // Map NDC XY [-1,1] straight into pyramid level-0 texel space.
    // The NDC-to-screen scale and the screen-to-pyramid rescale cancel, so only the pyramid extent is needed.
    uint2 pyramidExtentTexels;
    depthPyramidImage.GetDimensions(pyramidExtentTexels.x, pyramidExtentTexels.y);
    float2 pyramidExtent = float2(pyramidExtentTexels);
    float2 aabbMinPyramid = ((aabbMinNdc.xy * float2(0.5f)) + 0.5f) * pyramidExtent;
    float2 aabbMaxPyramid = ((aabbMaxNdc.xy * float2(0.5f)) + 0.5f) * pyramidExtent;

    // Pick the mip where the footprint spans at most one level-0 texel (1.f guard avoids a
    // negative level when the footprint is sub-texel). At this level a single bilinear fetch
    // covers a 2x2 neighbourhood, so it fully overlaps the footprint.
    float2 footprint = aabbMaxPyramid - aabbMinPyramid;
    float level = ceil(log2(max(max(footprint.x, footprint.y), 1.f)));

    // Sample once at the footprint center. The min-reduction sampler returns the minimum (i.e.
    // furthest, in reverse-Z) depth across the covered texels — the furthest occluder.
    // Clamp the UV to [0,1] since the center can fall off-screen (negative) for items at the edge.
    float2 centerPyramid = (aabbMinPyramid + aabbMaxPyramid) * 0.5f;
    float2 uv = clamp(centerPyramid / pyramidExtent, float2(0.f), float2(1.f));

    // Visible if the nearest point is closer-or-equal than the furthest occluder.
    // The depth pre-pass renders the instances being tested here, so frontmost surface finds its own depth in the pyramid
    float furthestDepthInPyramid = depthPyramidImage.SampleLevel(minSampler, uv, level);
    return closestDepthInRi >= furthestDepthInPyramid;
}

void markRiIndex(SwRenderCommand* rc, SwRenderItem* ri, uint* risIndicesBuffer) {
    // Write the indices of to-draw render items to the draw instances indices buffer
    // Index into to-draw instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRiIndex;
    InterlockedAdd(rc->mRiCount, 1, offsetVisibleRiIndex);
    uint visibleRiIndex = rc->mFirstRi + offsetVisibleRiIndex;
    risIndicesBuffer[visibleRiIndex] = ri->mInstanceIndex;
}

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    uint risIndex = in.mDispatchThreadID.x;
    if (risIndex >= testPc.mRisLimit)
        return;
    SwRenderItem* ri = &testPc.mRisBuffer[risIndex];
    SwRenderCommand* rc = &testPc.mRcsBuffer[ri->mRcIndex];

    SwBounds bounds = testPc.mBoundsBuffer[rc->mBoundsIndex];
    SwNode::Transform* nodeTransform = &testPc.mNodeTransformsBuffer[rc->mNodeTransformIndex];
    SwInstance::Data* instance = &testPc.mInstancesBuffer[ri->mInstanceIndex];
    SwFrustum frustum = testPc.mFrameBuffer->mCameraBuffer->mFrustum;
    SwCamera::Perspective* perspective = &testPc.mFrameBuffer->mCameraBuffer->mPerspective;
    float4[SwMesh::NUM_AABB_CORNERS] aabbCorners = {
        float4(bounds.mMin.x, bounds.mMin.y, bounds.mMin.z, 1.f),
        float4(bounds.mMin.x, bounds.mMin.y, bounds.mMax.z, 1.f),
        float4(bounds.mMin.x, bounds.mMax.y, bounds.mMin.z, 1.f),
        float4(bounds.mMin.x, bounds.mMax.y, bounds.mMax.z, 1.f),
        float4(bounds.mMax.x, bounds.mMin.y, bounds.mMin.z, 1.f),
        float4(bounds.mMax.x, bounds.mMin.y, bounds.mMax.z, 1.f),
        float4(bounds.mMax.x, bounds.mMax.y, bounds.mMin.z, 1.f),
        float4(bounds.mMax.x, bounds.mMax.y, bounds.mMax.z, 1.f),
    };

    transformCornersToWorld(aabbCorners, instance, nodeTransform);

    // Captured before occlusionCull rewrites the corners into NDC space.
    float3 worldAabbMin;
    float3 worldAabbMax;
    SwBounds::fromCorners(aabbCorners, worldAabbMin, worldAabbMax);

    uint riVisIndex = rc->mFirstRi + (ri->mInstanceIndex - rc->mFirstInstance);

    bool draw = false;
    if (testPc.mPhase == SwCull::Phase::Early) {
        // Draw whatever opaque was visible last frame (frustum only). Do not modify visibility.
        if (testPc.mBatchesBuffer[rc->mBatchIndex].mMaterialType == SwMaterial::Type::Opaque) {
            bool prevVisible = testPc.mRisVisibilityReadBuffer[riVisIndex];
            if (prevVisible)
                prevVisible = !SwCull::ENABLE_FRUSTUM_CULL || SwFrustum::aabbInFrustum(frustum, worldAabbMin, worldAabbMax);
            if (prevVisible) {
                markRiIndex(rc, ri, testPc.mRisIndicesBuffer);
                draw = true;
            }
        }
    } else {
        bool currVisible = (!SwCull::ENABLE_FRUSTUM_CULL || SwFrustum::aabbInFrustum(frustum, worldAabbMin, worldAabbMax)) &&
                           (!SwCull::ENABLE_OCCLUSION_CULL || occlusionCull(aabbCorners, perspective, depthPyramidImage));
        testPc.mRisVisibilityWriteBuffer[riVisIndex] = currVisible;
        // Opaque render commands are the only ones drawn by the early geometry pass, so only they emit just the newly-visible delta.
        bool hasEarlyDraw = testPc.mBatchesBuffer[rc->mBatchIndex].mMaterialType == SwMaterial::Type::Opaque;
        bool drawnEarly = hasEarlyDraw && testPc.mRisVisibilityReadBuffer[riVisIndex];
        if (currVisible && !drawnEarly) {
            markRiIndex(rc, ri, testPc.mRisIndicesBuffer);
            draw = true;
        }
    }
    uint _;
    if (draw)
        InterlockedAdd(*testPc.mRisCount, 1, _);
}
