    import SwCull;

    [[vk::binding(0, 0)]]
    public uniform Texture2D<float> depthFullImage;
    [[vk::binding(1, 0)]]
    public uniform Texture2D<float> depthPyramidRead[];
    [[vk::binding(2, 0)]]
    public uniform RWTexture2D<float> depthPyramidWrite[];
    [[vk::binding(3, 0)]]
    public uniform SamplerState minSampler;

    [[vk::push_constant]]
    SwCull::PrepOcclusionPC prepOcclusionPc;

    [numthreads(SwCull::MAX_2D_WORKGROUP_THREADS, SwCull::MAX_2D_WORKGROUP_THREADS, 1)]
    void main(SwCull::CSInput in) {
        uint2 writePos = in.mDispatchThreadID.xy;

        if (prepOcclusionPc.mLevel == -1) {
            // Dispatched at pyramid level 0 size. Each destination texel covers 1-2 full-depth
            // texels per axis (pyramidExtent = previousPow2(depthExtent) so invRatio < 2).
            // A min-reduction sampler would not cover all source texels reliably at non-2:1
            // ratios, so the source region is sampled explicitly.
            if (any(writePos >= prepOcclusionPc.mDepthPyramidExtent))
                return;

            float2 invRatio = float2(prepOcclusionPc.mDepthFullExtent) / float2(prepOcclusionPc.mDepthPyramidExtent);
            uint2 srcMin = uint2(float2(writePos) * invRatio);
            uint2 srcMax = min(uint2(float2(writePos + 1u) * invRatio), prepOcclusionPc.mDepthFullExtent - 1u);

            float minDepth = 1.0f;
            for (uint sy = srcMin.y; sy <= srcMax.y; sy++) {
                for (uint sx = srcMin.x; sx <= srcMax.x; sx++) {
                    minDepth = min(minDepth, depthFullImage.Load(uint3(sx, sy, 0)));
                }
            }
            depthPyramidWrite[0].Store(writePos, minDepth);
        } else {
            // Dispatched at level (readLevel+1) size. The min-reduction sampler with bilinear
            // filtering at the exact midpoint between each 2x2 source block produces the min
            // of those four texels in a single tap, including correct clamp-to-edge at odd
            // source dimensions.
            uint readLevel = prepOcclusionPc.mLevel;
            uint writeLevel = readLevel + 1u;

            uint2 writeLevelExtent = max(prepOcclusionPc.mDepthPyramidExtent >> writeLevel, uint2(1u, 1u));
            if (any(writePos >= writeLevelExtent))
                return;

            uint2 readExtent = max(prepOcclusionPc.mDepthPyramidExtent >> readLevel, uint2(1u, 1u));
            float2 uv = (float2(writePos) * 2.0f + 1.0f) / float2(readExtent);
            float minDepth = depthPyramidRead[readLevel].SampleLevel(minSampler, uv, 0.0f);

            depthPyramidWrite[writeLevel].Store(writePos, minDepth);
        }
    }
