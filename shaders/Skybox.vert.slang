import Skybox;
import Perspective;

[[vk::binding(0, 1)]]
public uniform SamplerCube skyboxCubemap;

[[vk::push_constant]]
SkyboxPushConstants skyboxPc;

VSOutput main(VSInput in)
{
    VSOutput out;

    float4 position = skyboxPc.skyboxVertexBuffer[in.vertexIndex].position;
    float4x4 viewWithoutTranslation =
        float4x4(float4(skyboxPc.perFrameBuffer[0].view[0].xyz, 0), float4(skyboxPc.perFrameBuffer[0].view[1].xyz, 0),
                 float4(skyboxPc.perFrameBuffer[0].view[2].xyz, 0), float4(0, 0, 0, 1));

    out.position = mul(mul(skyboxPc.perFrameBuffer[0].proj, viewWithoutTranslation), position);
    out.uvw = position.xyz;
    return out;
}
