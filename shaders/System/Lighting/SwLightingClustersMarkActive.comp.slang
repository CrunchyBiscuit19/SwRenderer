import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthImage;

[[vk::push_constant]]
SwLighting::ClustersMarkActivePC clustersMarkActivePc;

uint getDepthSlice(float z) {
    uint numSlices = SwLighting::CLUSTERS_DIMENSIONS.z;
    float near = clustersMarkActivePc.mFrameBuffer.mCameraBuffer->mPlanes.x;
    float far = clustersMarkActivePc.mFrameBuffer.mCameraBuffer->mPlanes.y;
    
    float numerator = (numSlices) * (log10(z) - log10(near));
    float denominator = log10(far / near);
    
    return floor(numerator / denominator);   
}

uint getClusterIndex(float3 texelCoord, uint2 depthImageExtents){
    const uint clusterDepthSlice = getDepthSlice(texelCoord.z);
    const uint tileSize = depthImageExtents.x / SwLighting::CLUSTERS_DIMENSIONS.x;
    uint3 cluster = uint3(uint2(texelCoord.xy / tileSize), clusterDepthSlice);

    return cluster.x + SwLighting::CLUSTERS_DIMENSIONS.x * cluster.y + (SwLighting::CLUSTERS_DIMENSIONS.x * SwLighting::CLUSTERS_DIMENSIONS.y) * cluster.z;
}

[numthreads(SwConstants::MAX_2D_WORKGROUP_THREADS, SwConstants::MAX_2D_WORKGROUP_THREADS, 1)]
void main(SwLighting::CSInput in) {
    uint2 depthImageExtents;
    depthImage.GetDimensions(depthImageExtents.x, depthImageExtents.y);
    
    if (any(in.mDispatchThreadID.xy >= depthImageExtents)) return;

    float z = depthImage.Load(uint3(in.mDispatchThreadID.xy, 0));
    uint clusterIndex = getClusterIndex(float3(in.mDispatchThreadID.xy, z), depthImageExtents);
    
    clustersMarkActivePc.mClustersActiveBooleansBuffer[clusterIndex] = true;
}
