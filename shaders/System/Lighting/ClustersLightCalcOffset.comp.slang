import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ClustersLightCalcOffsetPC clustersLightCalcOffsetPc;

[numthreads(SwConstants::MAX_2D_WORKGROUP_THREADS, SwConstants::MAX_2D_WORKGROUP_THREADS, 1)]
void main(SwLighting::CSInput in) {
    const uint activeSlot = in.mDispatchThreadID.x;
    const uint lightIndex = in.mDispatchThreadID.y;
    const uint activeCount = clustersLightCalcOffsetPc.mClustersActiveIndicesBuffer[0];
    if (activeSlot >= activeCount || lightIndex >= clustersLightCalcOffsetPc.mLightsCount) {
        return;
    }

    const uint clusterIndex = clustersLightCalcOffsetPc.mClustersActiveIndicesBuffer[1 + activeSlot];

    SwLight::Data light = clustersLightCalcOffsetPc.mLightsBuffer[lightIndex];
    SwLighting::Cluster cluster = clustersLightCalcOffsetPc.mClustersBuffer[clusterIndex];
    float4x4 view = clustersLightCalcOffsetPc.mFrameBuffer->mCameraBuffer->mPerspective.mView;
    if (!SwLighting::Collision::lightTouchesCluster(
            light, cluster, view, clustersLightCalcOffsetPc.mInstancesBuffer, clustersLightCalcOffsetPc.mNodeTransformsBuffer
        )) {
        return;
    }

    InterlockedAdd(clustersLightCalcOffsetPc.mClustersLightCounts[clusterIndex], 1);
}
