import System.IBL.SwIBL;
import Data.SwConstants;

[[vk::binding(0, 0)]]
public uniform Sampler2D hdrMap;

float4 main(SwIBL::FSInput in) : SV_Target {
    float3 dir = normalize(in.mUvw);
    float phi = atan2(dir.z, dir.x);
    float theta = asin(dir.y);
    float2 uv = float2(phi / (2.0 * SwConstants::PI) + 0.5, theta / SwConstants::PI + 0.5);
    return hdrMap.Sample(uv);
}
