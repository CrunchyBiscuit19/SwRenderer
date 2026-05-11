module Composite;

public struct VSInput
{
    public uint vertexIndex : SV_VulkanVertexID;
    public uint instanceIndex : SV_VulkanInstanceID;
    public uint drawIndex : SV_DrawIndex;
};

public struct VSOutput
{
    public float4 position : SV_Position;
};

public typedef VSOutput FSInput;

public struct FSOutput
{
    public float4 color : SV_Target0;
};