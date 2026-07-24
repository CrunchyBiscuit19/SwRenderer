import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ClustersLightPrefixSumOffsetPC clustersLightPrefixSumOffsetPc;

static const uint ELEMENTS_PER_THREAD = (SwLighting::NUM_CLUSTERS + SwConstants::MAX_1D_WORKGROUP_THREADS - 1) / SwConstants::MAX_1D_WORKGROUP_THREADS;

groupshared uint partialSums[SwConstants::MAX_1D_WORKGROUP_THREADS];

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    const uint threadIndex = in.mDispatchThreadID.x;
    const uint base = threadIndex * ELEMENTS_PER_THREAD;

    uint threadTotal = 0;
    for (uint i = 0; i < ELEMENTS_PER_THREAD; i++) {
        const uint clusterIndex = base + i;
        if (clusterIndex < SwLighting::NUM_CLUSTERS) {
            threadTotal += clustersLightPrefixSumOffsetPc.mClustersLightCounts[clusterIndex];
        }
    }

    partialSums[threadIndex] = threadTotal;
    GroupMemoryBarrierWithGroupSync();

    // Hillis and Steele inclusive scan over the per thread totals.
    for (uint offset = 1; offset < SwConstants::MAX_1D_WORKGROUP_THREADS; offset <<= 1) {
        uint addend = 0;
        if (threadIndex >= offset) {
            addend = partialSums[threadIndex - offset];
        }
        GroupMemoryBarrierWithGroupSync();
        if (threadIndex >= offset) {
            partialSums[threadIndex] += addend;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    uint running = partialSums[threadIndex] - threadTotal;

    for (uint i = 0; i < ELEMENTS_PER_THREAD; i++) {
        const uint clusterIndex = base + i;
        if (clusterIndex < SwLighting::NUM_CLUSTERS) {
            clustersLightPrefixSumOffsetPc.mClustersLightOffsetsBuffer[clusterIndex] = running;
            running += clustersLightPrefixSumOffsetPc.mClustersLightCounts[clusterIndex];
        }
    }
}
