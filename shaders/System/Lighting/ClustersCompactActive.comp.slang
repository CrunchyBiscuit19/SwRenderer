import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ClustersCompactActivePC clustersCompactActivePc;

[numthreads(SwConstants::MAX_1D_WORKGROUP_THREADS, 1, 1)]
void main(SwLighting::CSInput in) {
    uint clusterBooleanIndex = in.mDispatchThreadID.x;

    bool clusterActive = clustersCompactActivePc.mClustersActiveBooleansBuffer[clusterBooleanIndex];
    if (!clusterActive) return;

    uint clusterIndicesIndex;
    InterlockedAdd(clustersCompactActivePc.mClustersActiveIndicesBuffer[0], 1, clusterIndicesIndex);

    clustersCompactActivePc.mClustersActiveIndicesBuffer[1 + clusterIndicesIndex] = clusterBooleanIndex;
}
