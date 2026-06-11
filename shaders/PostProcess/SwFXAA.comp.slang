import SwPostProcess;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Texture2D<float4> inputImage;
[[vk::binding(1, 0)]]
public uniform SamplerState linearSampler;
[[vk::binding(2, 0)]]
public uniform RWTexture2D<float4> outputImage;

[[vk::push_constant]]
SwPostProcess::FxaaPC fxaaPc;

float3 sampleRgb(float2 uv) {
    return inputImage.SampleLevel(linearSampler, uv, 0.0f).rgb;
}

[numthreads(Sw::MAX_2D_WORKGROUP_THREADS, Sw::MAX_2D_WORKGROUP_THREADS, 1)]
void main(SwPostProcess::CSInput in) {
    uint2 pixel = in.mDispatchThreadID.xy;

    uint2 extent;
    outputImage.GetDimensions(extent.x, extent.y);
    if (any(pixel >= extent))
        return;

    float2 texel = fxaaPc.mInverseScreenSize;
    float2 uv = (float2(pixel) + 0.5f) * texel;

    float4 center = inputImage.SampleLevel(linearSampler, uv, 0.0f);
    float3 rgbM = center.rgb;
    float3 rgbNW = sampleRgb(uv + float2(-1.0f, -1.0f) * texel);
    float3 rgbNE = sampleRgb(uv + float2(1.0f, -1.0f) * texel);
    float3 rgbSW = sampleRgb(uv + float2(-1.0f, 1.0f) * texel);
    float3 rgbSE = sampleRgb(uv + float2(1.0f, 1.0f) * texel);

    float lumaM = dot(rgbM, SwPostProcess::LUMA);
    float lumaNW = dot(rgbNW, SwPostProcess::LUMA);
    float lumaNE = dot(rgbNE, SwPostProcess::LUMA);
    float lumaSW = dot(rgbSW, SwPostProcess::LUMA);
    float lumaSE = dot(rgbSE, SwPostProcess::LUMA);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    // Flat regions carry no edge to smooth, so leave the original color untouched.
    if (lumaRange < max(SwPostProcess::EDGE_THRESHOLD_MIN, lumaMax * SwPostProcess::EDGE_THRESHOLD)) {
        outputImage.Store(pixel, center);
        return;
    }

    // Edge direction is perpendicular to the local SwPostProcess::LUMA gradient, scaled into UV space.
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25f * SwPostProcess::REDUCE_MUL, SwPostProcess::REDUCE_MIN);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -SwPostProcess::SPAN_MAX, SwPostProcess::SPAN_MAX) * texel;

    // Narrow blend (rgbA) plus a wider pair (rgbB); the wide blend is dropped when it overshoots the local SwPostProcess::LUMA envelope, which keeps high-contrast edges from smearing.
    float3 rgbA = 0.5f * (sampleRgb(uv + dir * (1.0f / 3.0f - 0.5f)) + sampleRgb(uv + dir * (2.0f / 3.0f - 0.5f)));
    float3 rgbB = rgbA * 0.5f + 0.25f * (sampleRgb(uv + dir * -0.5f) + sampleRgb(uv + dir * 0.5f));

    float lumaB = dot(rgbB, SwPostProcess::LUMA);
    float3 result = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;

    outputImage.Store(pixel, float4(result, center.a));
}
