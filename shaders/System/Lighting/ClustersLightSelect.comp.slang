import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ClustersLightSelectPC clustersLightSelectPc;

[numthreads(SwConstants::MAX_2D_WORKGROUP_THREADS, SwConstants::MAX_2D_WORKGROUP_THREADS, 1)]
void main(SwLighting::CSInput in) {
    const uint activeSlot = in.mDispatchThreadID.x;
    const uint lightIndex = in.mDispatchThreadID.y;
    const uint activeCount = clustersLightSelectPc.mClustersActiveIndicesBuffer[0];
    if (activeSlot >= activeCount || lightIndex >= clustersLightSelectPc.mLightsCount) {
        return;
    }

    const uint clusterIndex = clustersLightSelectPc.mClustersActiveIndicesBuffer[1 + activeSlot];

    SwLight::Data light = clustersLightSelectPc.mLightsBuffer[lightIndex];
    SwLighting::Cluster cluster = clustersLightSelectPc.mClustersBuffer[clusterIndex];
    float4x4 view = clustersLightSelectPc.mFrameBuffer->mCameraBuffer->mPerspective.mView;
    if (!SwLighting::Collision::lightTouchesCluster(
            light, cluster, view, clustersLightSelectPc.mInstancesBuffer, clustersLightSelectPc.mNodeTransformsBuffer
        )) {
        return;
    }

    uint slotInBlock;
    InterlockedAdd(clustersLightSelectPc.mClustersLightWriteCursorsBuffer[clusterIndex], 1, slotInBlock);
    const uint writeIndex = clustersLightSelectPc.mClustersLightOffsetsBuffer[clusterIndex] + slotInBlock;
    clustersLightSelectPc.mClustersLightIndicesBuffer[writeIndex] = lightIndex;
}
