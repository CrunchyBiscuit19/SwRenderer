module Skybox;

import Perspective;

public struct SkyboxVertex
{
    public float4 position;
};
public struct SkyboxPushConstants
{
    public SkyboxVertex *skyboxVertexBuffer;
};

public struct VSInput
{
    public uint vertexIndex : SV_VulkanVertexID;
}
public struct VSOutput
{
    public float4 position : SV_Position;
    public float3 uvw : TEXCOORD0;
};

public typedef VSOutput FSInput;
