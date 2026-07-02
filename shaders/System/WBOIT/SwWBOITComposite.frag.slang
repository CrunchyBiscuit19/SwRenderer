import SwWBOIT;

[[vk::binding(0, 0)]]
public uniform Texture2D<float4> accumImage;
[[vk::binding(1, 0)]]
public uniform Texture2D<float> rvlImage;

const static float EPSILON = 0.00001f;

bool isApproximatelyEqual(float a, float b) {
    return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
}

float maxOf3(float3 v) {
    return max(max(v.x, v.y), v.z);
}

SwWBOIT::FSOutput main(SwWBOIT::FSInput in) : SV_Target {
    SwWBOIT::FSOutput out;

    uint3 coords = uint3(uint2(in.mPosition.xy), 0);
    
    float rvl = rvlImage.Load(coords);
    if (isApproximatelyEqual(rvl, 1.f)) discard;

    float4 accum = accumImage.Load(coords);
    if (isinf(maxOf3(abs(accum.rgb)))) accum.rgb = float3(accum.a);

    float3 averageColor = accum.rgb / max(accum.a, EPSILON);
    out.mColor = float4(averageColor, 1.f - rvl);

    return out;
}