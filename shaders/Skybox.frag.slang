import Skybox;
import Perspective;

[[vk::binding(0, 0)]]
public uniform SamplerCube skyboxCubemap;

float4 main(FSInput in) : SV_Target {
    return skyboxCubemap.Sample(in.uvw);
}
