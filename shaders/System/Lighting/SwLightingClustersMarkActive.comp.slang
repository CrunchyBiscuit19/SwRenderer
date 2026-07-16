import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthImage;

[[vk::push_constant]]
SwLighting::ClustersMarkActivePC clustersMarkActivePc;

uint getDepthSlice(float ndcZ) {
    uint numSlices = SwLighting::CLUSTERS_DIMENSIONS.z;
    float near = clustersMarkActivePc.mFrameBuffer.mCameraBuffer->mPlanes.x;
    float far = clustersMarkActivePc.mFrameBuffer.mCameraBuffer->mPlanes.y;

    float viewZ = far * near / (near + ndcZ * (far - near));

    float numerator = (numSlices) * (log(viewZ) - log(near));
    float denominator = log(far / near);

    return min(uint(floor(numerator / denominator)), numSlices - 1);
}

uint getClusterIndex(float2 screenPixel, float ndcZ, uint2 depthImageExtents) {
    const uint clusterDepthSlice = getDepthSlice(ndcZ);
    const uint tileSize = depthImageExtents.x / SwLighting::CLUSTERS_DIMENSIONS.x;
    uint3 cluster = uint3(uint2(screenPixel / tileSize), clusterDepthSlice);

    return cluster.x + SwLighting::CLUSTERS_DIMENSIONS.x * cluster.y + (SwLighting::CLUSTERS_DIMENSIONS.x * SwLighting::CLUSTERS_DIMENSIONS.y) * cluster.z;
}

[numthreads(SwConstants::MAX_2D_WORKGROUP_THREADS, SwConstants::MAX_2D_WORKGROUP_THREADS, 1)]
void main(SwLighting::CSInput in) {
    uint2 depthImageExtents;
    depthImage.GetDimensions(depthImageExtents.x, depthImageExtents.y);

    if (any(in.mDispatchThreadID.xy >= depthImageExtents)) return;

    float z = depthImage.Load(uint3(in.mDispatchThreadID.xy, 0));
    if (z == 0.f) return;  // Reversed-Z cleared background has no geometry to cluster.

    float2 screenPixel = float2(in.mDispatchThreadID.xy);
    uint clusterIndex = getClusterIndex(screenPixel, z, depthImageExtents);

    clustersMarkActivePc.mClustersActiveBooleansBuffer[clusterIndex] = true;
}
