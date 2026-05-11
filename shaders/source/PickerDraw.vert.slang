import Picker;
import Vertex;
import Perspective;
import RenderItem;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;

[[vk::push_constant]]
PickerDrawPushConstants pickerDrawPc;

VSOutput main(VSInput in) {
    VSOutput out;

    Vertex v = pickerDrawPc.mainVertexBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    RenderItem renderItem = pickerDrawPc.postCullRenderItemsBuffer[in.drawIndex];

    uint firstRenderInstance = renderItem.firstRenderInstance;
    uint mainInstanceIndex = pickerDrawPc.mainVisibleRenderInstancesInstanceIndexBuffer[in.instanceIndex];
    float4x4 instanceTransformMatrix = pickerDrawPc.mainInstancesBuffer[mainInstanceIndex].transformMatrix;

    uint mainNodeTransformIndex = pickerDrawPc.postCullRenderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = pickerDrawPc.mainNodeTransformsBuffer[mainNodeTransformIndex];

    out.position = mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix), position);
    out.modelIndex = pickerDrawPc.postCullRenderItemsBuffer[in.drawIndex].modelIndex;
    out.instanceIndex = mainInstanceIndex;
    return out;
}
