import SwSkybox;

[[vk::binding(0, 0)]]
public uniform SamplerCube cubemap;

float4 main(SwSkybox::FSInput in) : SV_Target {
    return cubemap.Sample(in.mUvw);
}
