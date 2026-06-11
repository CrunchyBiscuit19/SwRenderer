import SwCommon;
import SwPostProcess;

[[vk::binding(0, 0)]]
public uniform RWTexture2D<float4> drawImage;

[[vk::push_constant]]
SwPostProcess::TonemapPC tonemapPc;

float3 acesFilmic(float3 x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[numthreads(Sw::MAX_2D_WORKGROUP_THREADS, Sw::MAX_2D_WORKGROUP_THREADS, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;

    uint2 extent;
    drawImage.GetDimensions(extent.x, extent.y);
    if (any(pixel >= extent))
        return;

    float4 hdr = drawImage[pixel];
    float3 mapped = acesFilmic(hdr.rgb * tonemapPc.mExposure);
    drawImage[pixel] = float4(mapped, hdr.a);
}
