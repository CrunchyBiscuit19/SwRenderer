import Geometry;
import Vertex;
import MaterialConstant;
import RenderItem;
import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform Sampler2D mFaterialResources[];

[[vk::push_constant]]
GeometryPushConstants geometryPc;

VSOutput main(VSInput in) {
    VSOutput out;

    Vertex v = geometryPc.mainVertexBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    RenderItem renderItem = geometryPc.postCullRenderItemsBuffer[in.drawIndex];

    uint firstRenderInstance = renderItem.firstRenderInstance;
    uint mainInstanceIndex = geometryPc.mainVisibleRenderInstancesInstanceIndexBuffer[in.instanceIndex];
    float4x4 instanceTransformMatrix = geometryPc.mainInstancesBuffer[mainInstanceIndex].transformMatrix;

    uint mainNodeTransformIndex = geometryPc.postCullRenderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = geometryPc.mainNodeTransformsBuffer[mainNodeTransformIndex];

    uint mainMaterialIndex = geometryPc.postCullRenderItemsBuffer[in.drawIndex].materialIndex;
    MaterialConstant materialConstant = geometryPc.mainMaterialConstantsBuffer[mainMaterialIndex];

    out.position = mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix), position);
    out.normal = v.normal;
    out.uv = float2(v.uv_x, v.uv_y);
    out.color = v.color * materialConstant.baseFactor;
    out.materialIndex = mainMaterialIndex;
    return out;
}
