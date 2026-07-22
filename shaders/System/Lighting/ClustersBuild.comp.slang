import System.Lighting.SwLighting;
import Data.SwConstants;

[[vk::push_constant]]
SwLighting::ClustersBuildPC clustersBuildPc;

float3 screenToView(float4x4 invProj, float2 screenPixel, float ndcZ, uint2 targetSize) {
    float2 uv = screenPixel / float2(targetSize);
    float4 clip = float4(uv * 2.f - 1.f, ndcZ, 1.f);  // Vulkan NDC y-down matches top-down pixels, no extra flip
    float4 view = mul(invProj, clip);
    return view.xyz / view.w;
}

float3 lineIntersectionToZPlane(float3 eyePos, float3 viewPoint, float tileZ) {
    float3 dir = viewPoint - eyePos;
    float t = (tileZ - eyePos.z) / dir.z;  // plane normal is (0, 0, 1)
    //float t = (tileZ - dot((0,0,1), eyePos)) / dot((0,0,1), viewPoint - eyePos);
    return eyePos + t * dir;
}

[numthreads(SwConstants::MAX_3D_WORKGROUP_THREADS, SwConstants::MAX_3D_WORKGROUP_THREADS, SwConstants::MAX_3D_WORKGROUP_THREADS)]
void main(SwLighting::CSInput in) {
    if (any(in.mDispatchThreadID >= SwLighting::CLUSTERS_DIMENSIONS)) return;

    const float3 eyePos = float3(0.f); //Eye position is zero in view space
    const uint clusterIndex =
        in.mDispatchThreadID.x + in.mDispatchThreadID.y * SwLighting::CLUSTERS_DIMENSIONS.x +
        in.mDispatchThreadID.z * SwLighting::CLUSTERS_DIMENSIONS.x * SwLighting::CLUSTERS_DIMENSIONS.y;
    const uint tileSize = clustersBuildPc.mTargetSize.x / SwLighting::CLUSTERS_DIMENSIONS.x;

    // Calculating the min and max point in screen space. Under reversed-Z the near plane is NDC z = 1.
    float2 minPointScreen = in.mDispatchThreadID.xy * tileSize;
    float2 maxPointScreen = (in.mDispatchThreadID.xy + float2(1.f)) * tileSize;

    // Pass min and max to view space
    float3 maxPointView = screenToView(clustersBuildPc.mInvProj, maxPointScreen, 1.f, clustersBuildPc.mTargetSize);
    float3 minPointView = screenToView(clustersBuildPc.mInvProj, minPointScreen, 1.f, clustersBuildPc.mTargetSize);

    // Near and far values of the cluster in view space
    float zNear     = clustersBuildPc.mFrameBuffer.mCameraBuffer.mPlanes.x;
    float zFar      = clustersBuildPc.mFrameBuffer.mCameraBuffer.mPlanes.y;
    float tileNear  = -zNear * pow(zFar/ zNear, in.mDispatchThreadID.z / float(SwLighting::CLUSTERS_DIMENSIONS.z));
    float tileFar   = -zNear * pow(zFar/ zNear, (in.mDispatchThreadID.z + 1) /float(SwLighting::CLUSTERS_DIMENSIONS.z));

    //Finding the 4 intersection points made from each point to the cluster near/far plane
    float3 minPointNear = lineIntersectionToZPlane(eyePos, minPointView, tileNear);
    float3 minPointFar  = lineIntersectionToZPlane(eyePos, minPointView, tileFar);
    float3 maxPointNear = lineIntersectionToZPlane(eyePos, maxPointView, tileNear);
    float3 maxPointFar  = lineIntersectionToZPlane(eyePos, maxPointView, tileFar);

    float3 minPointAABB = min(min(minPointNear, minPointFar),min(maxPointNear, maxPointFar));
    float3 maxPointAABB = max(max(minPointNear, minPointFar),max(maxPointNear, maxPointFar));

    clustersBuildPc.mClustersBuffer[clusterIndex].mMin = minPointAABB;
    clustersBuildPc.mClustersBuffer[clusterIndex].mMax = maxPointAABB;
}