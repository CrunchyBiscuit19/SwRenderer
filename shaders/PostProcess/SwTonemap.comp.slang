import SwCommon;
import SwPostProcess;

[[vk::binding(0, 0)]]
public uniform RWTexture2D<float4> drawImage;

[[vk::push_constant]]
SwPostProcess::Tonemap::WorkPC tonemapPc;

[numthreads(Sw::MAX_2D_WORKGROUP_THREADS, Sw::MAX_2D_WORKGROUP_THREADS, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;

    uint2 extent;
    drawImage.GetDimensions(extent.x, extent.y);
    if (any(pixel >= extent))
        return;

    float4 hdr = drawImage[pixel];
    float3 mapped = SwPostProcess::Tonemap::acesTonemap(hdr.rgb * tonemapPc.mExposure);
    drawImage[pixel] = float4(mapped, hdr.a);
}
