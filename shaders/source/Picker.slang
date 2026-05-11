module Picker;

import Perspective;
import Vertex;
import NodeTransform;
import Instance;
import RenderItem;

public struct PickerDrawPushConstants
{
    public Vertex *mainVertexBuffer;
    public NodeTransform *mainNodeTransformsBuffer;
    public Instance *mainInstancesBuffer;
    public uint *mainVisibleRenderInstancesInstanceIndexBuffer;
    public RenderItem *postCullRenderItemsBuffer;
};

public struct VSInput
{
    public uint vertexIndex : SV_VulkanVertexID;
    public uint instanceIndex : SV_VulkanInstanceID;
    public uint drawIndex : SV_DrawIndex;
};

public struct VSOutput
{
    public float4 position : SV_Position;
    public nointerpolation uint modelIndex;
    public nointerpolation uint instanceIndex;
};

public typedef VSOutput FSInput;

public struct PickerData {
    public int2 coords;
    public uint2 read;
};
public struct PickerPickPushConstants {
    public PickerData* pickerBuffer;
};

public struct CSInput {
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};
