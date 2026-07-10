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

bool frustumTest(SwFrustum::Data* frustum, float4[SwMesh::NUM_AABB_CORNERS] aabbCorners) {
    bool visible = true;

    // Get the min and max corners of the (already world-space) AABB
    float4 aabbMin = aabbCorners[0];
    float4 aabbMax = aabbCorners[0];
    for (uint j = 1; j < SwMesh::NUM_AABB_CORNERS; j++) {
        aabbMin = min(aabbMin, aabbCorners[j]);
        aabbMax = max(aabbMax, aabbCorners[j]);
    }

    // For each plane build the corner which will produce max result in point-normal plane equation
    // Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
    // Frustum plane normals point inward => non-negative result means at least 1 corner is on correct side of the plane
    // At least 1 corner on correct side of all 6 planes means render item is within frustum and therefore visible
    const SwFrustum::Plane* planes[SwFrustum::NUM_FRUSTUM_PLANES] = {
        &frustum->mNear, &frustum->mFar, &frustum->mLeft, &frustum->mRight, &frustum->mTop, &frustum->mBottom
    };
    for (uint i = 0; i < SwFrustum::NUM_FRUSTUM_PLANES; i++) {
        SwFrustum::Plane* p = planes[i];
        float4 chosen;
        chosen.x = (p->mNormal.x >= 0) ? aabbMax.x : aabbMin.x;
        chosen.y = (p->mNormal.y >= 0) ? aabbMax.y : aabbMin.y;
        chosen.z = (p->mNormal.z >= 0) ? aabbMax.z : aabbMin.z;
        chosen.w = 1.0;
        float result = dot(p->mNormal, chosen.xyz) - p->mDistance;
        if (result < 0) {
            visible = false;
            break;
        }
    }
    return visible;
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

void markRiIndex(SwBatch::RenderCommand* rc, SwBatch::RenderItem* ri, uint* sceneRisIndicesBuffer) {
    // Write the indices of to-draw render items to the draw instances indices buffer
    // Index into to-draw instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRiIndex;
    InterlockedAdd(rc->mRiCount, 1, offsetVisibleRiIndex);
    uint visibleRiIndex = rc->mFirstRi + offsetVisibleRiIndex;
    sceneRisIndicesBuffer[visibleRiIndex] = ri->mSceneInstanceIndex;
}

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwCull::CSInput in) {
    uint risIndex = in.mDispatchThreadID.x;
    if (risIndex >= testPc.mSceneRisLimit)
        return;
    SwBatch::RenderItem* ri = &testPc.mSceneRisBuffer[risIndex];
    SwBatch::RenderCommand* rc = &testPc.mSceneRcsBuffer[ri->mRcIndex];

    SwMesh::Bounds* bounds = &testPc.mSceneBoundsBuffer[rc->mBoundsIndex];
    SwNode::Transform* nodeTransform = &testPc.mSceneNodeTransformsBuffer[rc->mNodeTransformIndex];
    SwInstance::Data* instance = &testPc.mSceneInstancesBuffer[ri->mSceneInstanceIndex];
    SwFrustum::Data* frustum = &testPc.mFrameBuffer->mCameraBuffer->mFrustum;
    SwCamera::Perspective* perspective = &testPc.mFrameBuffer->mCameraBuffer->mPerspective;
    float4[SwMesh::NUM_AABB_CORNERS] aabbCorners = {
        float4(bounds->mMin.x, bounds->mMin.y, bounds->mMin.z, 1.f),
        float4(bounds->mMin.x, bounds->mMin.y, bounds->mMax.z, 1.f),
        float4(bounds->mMin.x, bounds->mMax.y, bounds->mMin.z, 1.f),
        float4(bounds->mMin.x, bounds->mMax.y, bounds->mMax.z, 1.f),
        float4(bounds->mMax.x, bounds->mMin.y, bounds->mMin.z, 1.f),
        float4(bounds->mMax.x, bounds->mMin.y, bounds->mMax.z, 1.f),
        float4(bounds->mMax.x, bounds->mMax.y, bounds->mMin.z, 1.f),
        float4(bounds->mMax.x, bounds->mMax.y, bounds->mMax.z, 1.f),
    };

    transformCornersToWorld(aabbCorners, instance, nodeTransform);

    uint riVisIndex = rc->mFirstRi + (ri->mSceneInstanceIndex - rc->mFirstInstance);

    bool draw = false;
    if (testPc.mPhase == SwCull::Phase::Early) {
        // Draw whatever was visible last frame (frustum only). Do not modify visibility.
        bool prevVisible = testPc.mSceneRisVisibilityReadBuffer[riVisIndex];
        if (prevVisible)
            prevVisible = !SwCull::ENABLE_FRUSTUM_CULL || frustumTest(frustum, aabbCorners);
        if (prevVisible) {
            markRiIndex(rc, ri, testPc.mSceneRisIndicesBuffer);
            draw = true;
        }
    } else {
        bool currVisible = (!SwCull::ENABLE_FRUSTUM_CULL || frustumTest(frustum, aabbCorners)) &&
                           (!SwCull::ENABLE_OCCLUSION_CULL || occlusionCull(aabbCorners, perspective, depthPyramidImage));
        testPc.mSceneRisVisibilityWriteBuffer[riVisIndex] = currVisible;
        // Opaque render commands are the only ones drawn by the early geometry pass, so only they emit just the newly-visible delta.
        bool hasEarlyDraw = testPc.mSceneBatchesBuffer[rc->mBatchIndex].mMaterialType == SwMaterial::Type::Opaque;
        bool drawnEarly = hasEarlyDraw && testPc.mSceneRisVisibilityReadBuffer[riVisIndex];
        if (currVisible && !drawnEarly) {
            markRiIndex(rc, ri, testPc.mSceneRisIndicesBuffer);
            draw = true;
        }
    }
    uint _;
    if (draw)
        InterlockedAdd(*testPc.mSceneRisCount, 1, _);
}
