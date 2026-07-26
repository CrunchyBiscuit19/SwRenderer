import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthImage;

[[vk::push_constant]]
SwLighting::ClustersMarkActivePC clustersMarkActivePc;

[numthreads(SwConstants::MAX_2D_WORKGROUP_THREADS, SwConstants::MAX_2D_WORKGROUP_THREADS, 1)]
void main(SwLighting::CSInput in) {
    uint2 depthImageExtents;
    depthImage.GetDimensions(depthImageExtents.x, depthImageExtents.y);

    if (any(in.mDispatchThreadID.xy >= depthImageExtents)) return;

    float z = depthImage.Load(uint3(in.mDispatchThreadID.xy, 0));
    if (z == 0.f) return;  // Reversed-Z cleared background has no geometry to cluster.

    float2 screenPixel = float2(in.mDispatchThreadID.xy);
    uint clusterIndex = SwLighting::getClusterIndex(screenPixel, z, depthImageExtents, clustersMarkActivePc.mFrameBuffer->mCameraBuffer->mPlanes);

    clustersMarkActivePc.mClustersActiveBooleansBuffer[clusterIndex] = true;
}
