import Culler;
import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

static const uint MAX_1D_WORKGROUP_THREADS = 1024;
static const uint NUM_FRUSTUM_PLANES = 6;
static const uint NUM_AABB_CORNERS = 8;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthPyramidImage;

[[vk::push_constant]]
CullerCullPushConstants cullerCullPc;

bool frustumCull(Plane* frustum, inout float4[NUM_AABB_CORNERS] aabbCorners, AABB* bounds, Instance* currInstance, NodeTransform* nodeTransform) {
    bool visible = true;

    // Transform all corners of the AABB to world space
    // Get the min and max corners of the AABB
    aabbCorners[0] = mul(currInstance->transformMatrix, mul(*nodeTransform, aabbCorners[0]));
    float4 aabbMin = aabbCorners[0];
    float4 aabbMax = aabbCorners[0];
    for (uint j = 1; j < NUM_AABB_CORNERS; j++) {
        aabbCorners[j] = mul(currInstance->transformMatrix, mul(*nodeTransform, aabbCorners[j]));
        aabbMin = min(aabbMin, aabbCorners[j]);
        aabbMax = max(aabbMax, aabbCorners[j]);
    }

    // For each plane build the corner which will produce max result in point-normal plane equation
    // Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
    // Frustum plane normals point inward => non-negative result means at least 1 corner is on correct side of the plane
    // At least 1 corner on correct side of all 6 planes means render instance is within frustum and therefore visible
    for (uint i = 0; i < NUM_FRUSTUM_PLANES; i++) {
        Plane p = frustum[i];
        float4 chosen;
        chosen.x = (p.normal.x >= 0) ? aabbMax.x : aabbMin.x;
        chosen.y = (p.normal.y >= 0) ? aabbMax.y : aabbMin.y;
        chosen.z = (p.normal.z >= 0) ? aabbMax.z : aabbMin.z;
        chosen.w = 1.0;
        float result = dot(p.normal, chosen.xyz) - p.d;
        if (result < 0) {
            visible = false;
            break;
        }
    }
    return visible;
}

bool occlusionCull(inout float4[NUM_AABB_CORNERS] aabbCorners, Perspective *perspective, Texture2D<float> depthPyramidImage, float2 drawExtents) {
    float4 aabbMinNdc = float4(1.f, 1.f, 1.f, 0.f);
    float4 aabbMaxNdc = float4(-1.f, -1.f, 0.f, 0.f);
    float closestDepthInRenderInstance = 0.f;

    // Transform AABB corners to NDC / Screen space and find min/max XY and closest Z
    // Vulkan NDC is [0, 1] for Z and [-1, 1] for XY
    // Reverse-Z in effect (1 == near, 0 == far)
    for (uint j = 0; j < NUM_AABB_CORNERS; j++) {
        float4 clipPos = mul(perspective->proj, mul(perspective->view, aabbCorners[j]));

        float4 ndc = clipPos / clipPos.w;
        aabbCorners[j] = ndc;
        aabbMinNdc = min(aabbMinNdc, ndc);
        aabbMaxNdc = max(aabbMaxNdc, ndc);

        closestDepthInRenderInstance = max(closestDepthInRenderInstance, ndc.z);
    }

    // Convert NDC XY from [-1,1] to [0, drawExtents], and clamp it
    // Clamping because AABB spills outside of screen, also increase margin to be safe
    float2 aabbMinScreen = ((aabbMinNdc.xy * float2(0.5f)) + 0.5f) * drawExtents;
    float2 aabbMaxScreen = ((aabbMaxNdc.xy * float2(0.5f)) + 0.5f) * drawExtents;

    // Compute information needed to look up in the depth pyramid
    uint width = max(uint(aabbMaxScreen.x - aabbMinScreen.x), 1u);
    uint height = max(uint(aabbMaxScreen.y - aabbMinScreen.y), 1u);
    uint2 topLeft = uint2((uint)aabbMinScreen.x, (uint)aabbMinScreen.y);
    uint2 topRight = uint2((uint)aabbMaxScreen.x, (uint)aabbMinScreen.y);
    uint2 bottomLeft = uint2((uint)aabbMinScreen.x, (uint)aabbMaxScreen.y);
    uint2 bottomRight = uint2((uint)aabbMaxScreen.x, (uint)aabbMaxScreen.y);

    // Adjusted for levels, then clamp within the mip size
    uint level = uint(floor(log2(float(max(width, height)))));
    uint2 mipSize = (cullerCullPc.depthPyramidExtents + (1u << level) - 1u) >> level;
    uint2 clampedTopLeft = clamp(topLeft >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedTopRight = clamp(topRight >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedBottomLeft = clamp(bottomLeft >> level, uint2(0, 0), mipSize - 1u);
    uint2 clampedBottomRight = clamp(bottomRight >> level, uint2(0, 0), mipSize - 1u);

    // Sample the pyramid using the center coordinates adjusted for mip level
    // Not perfect but whatever
    float d0 = depthPyramidImage.Load(uint3(clampedTopRight.x, clampedTopRight.y, level));
    float d1 = depthPyramidImage.Load(uint3(clampedTopLeft.x, clampedTopLeft.y, level));
    float d2 = depthPyramidImage.Load(uint3(clampedBottomLeft.x, clampedBottomLeft.y, level));
    float d3 = depthPyramidImage.Load(uint3(clampedBottomRight.x, clampedBottomRight.y, level));

    // Visible if closest is closer (greater than because of reverse-Z) than furthest depth
    float furthestDepthInPyramid = min(min(d0, d1), min(d2, d3));
    return closestDepthInRenderInstance > furthestDepthInPyramid;
}

[numthreads(MAX_1D_WORKGROUP_THREADS,1,1)]
void main(CSInput in) {
    // Get thread ID as index into which render instance for culling check
    uint renderInstancesIndex = in.dispatchThreadID.x;
    if (renderInstancesIndex >= cullerCullPc.renderInstancesLimit) return;

    // Grab representative data for this thread
    RenderInstance renderInstance = cullerCullPc.renderInstancesBuffer[renderInstancesIndex];
    RenderItem *renderItem = &cullerCullPc.preCullRenderItemsBuffer[renderInstance.renderItemsIndex];

    // Grab data for culling test
    Instance* instance = &cullerCullPc.mainInstancesBuffer[renderInstance.mainInstanceIndex];
    NodeTransform* nodeTransform = &cullerCullPc.mainNodeTransformsBuffer[renderItem->nodeTransformIndex];
    AABB* bounds = &cullerCullPc.mainBoundsBuffer[renderItem->boundsIndex];
    Plane* frustum = cullerCullPc.frustumBuffer;
    Perspective* perspective = cullerCullPc.perspectiveBuffer;

    // Establish AABB corners
    float4[NUM_AABB_CORNERS] aabbCorners = {
        float4(bounds->min.x, bounds->min.y, bounds->min.z, 1.f),
        float4(bounds->min.x, bounds->min.y, bounds->max.z, 1.f),
        float4(bounds->min.x, bounds->max.y, bounds->min.z, 1.f),
        float4(bounds->min.x, bounds->max.y, bounds->max.z, 1.f),
        float4(bounds->max.x, bounds->min.y, bounds->min.z, 1.f),
        float4(bounds->max.x, bounds->min.y, bounds->max.z, 1.f),
        float4(bounds->max.x, bounds->max.y, bounds->min.z, 1.f),
        float4(bounds->max.x, bounds->max.y, bounds->max.z, 1.f),
    };

    // Culling tests
    bool visible = frustumCull(frustum, aabbCorners, bounds, instance, nodeTransform);
    // if (visible) visible = occlusionCull(aabbCorners, perspective, depthPyramidImage, cullerCullPc.drawExtents);
    if (visible) occlusionCull(aabbCorners, perspective, depthPyramidImage, cullerCullPc.drawExtents);

    // Increment counter of unculled render instances
    if (!visible) return;
    uint _; InterlockedAdd(cullerCullPc.renderInstancesCountBuffer[0], 1, _);

    // Write the indices of visible render instances to the visible instances indices buffer
    // Index into visible instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRenderInstanceIndex;
    InterlockedAdd(renderItem->renderInstanceCount, 1, offsetVisibleRenderInstanceIndex);
    uint finalVisibleRenderInstanceIndex = renderItem->firstRenderInstance + offsetVisibleRenderInstanceIndex;
    cullerCullPc.mainVisibleRenderInstancesInstanceIndexBuffer[finalVisibleRenderInstanceIndex] = renderInstance.mainInstanceIndex;
}