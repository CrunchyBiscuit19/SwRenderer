import SwPostProcess;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Texture2D<float4> inputImage;
[[vk::binding(1, 0)]]
public uniform SamplerState linearSampler;
[[vk::binding(2, 0)]]
public uniform RWTexture2D<float4> outputImage;

[[vk::push_constant]]
SwPostProcess::Fxaa::WorkPC fxaaPc;

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
    float3 rgbNW = inputImage.SampleLevel(linearSampler, uv + float2(-1.0f, -1.0f) * texel, 0.0f).rgb;
    float3 rgbNE = inputImage.SampleLevel(linearSampler, uv + float2(1.0f, -1.0f) * texel, 0.0f).rgb;
    float3 rgbSW = inputImage.SampleLevel(linearSampler, uv + float2(-1.0f, 1.0f) * texel, 0.0f).rgb;
    float3 rgbSE = inputImage.SampleLevel(linearSampler, uv + float2(1.0f, 1.0f) * texel, 0.0f).rgb;

    float lumaM = dot(rgbM, SwPostProcess::LUMA);
    float lumaNW = dot(rgbNW, SwPostProcess::LUMA);
    float lumaNE = dot(rgbNE, SwPostProcess::LUMA);
    float lumaSW = dot(rgbSW, SwPostProcess::LUMA);
    float lumaSE = dot(rgbSE, SwPostProcess::LUMA);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(SwPostProcess::EDGE_THRESHOLD_MIN, lumaMax * SwPostProcess::EDGE_THRESHOLD)) {
        outputImage.Store(pixel, center);
        return;
    }

    // Edge directions.
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    // Normalize and clamp directions
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25f * SwPostProcess::REDUCE_MUL, SwPostProcess::REDUCE_MIN);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -SwPostProcess::SPAN_MAX, SwPostProcess::SPAN_MAX) * texel;

    // Calculate 2 blends, narrow and wide.
    float3 rgbNarrow = 0.5f * (inputImage.SampleLevel(linearSampler, uv + dir * (1.0f / 3.0f - 0.5f), 0.0f).rgb + inputImage.SampleLevel(linearSampler, uv + dir * (2.0f / 3.0f - 0.5f), 0.0f).rgb);
    float3 rgbWide = rgbNarrow * 0.5f + 0.25f * (inputImage.SampleLevel(linearSampler, uv + dir * -0.5f, 0.0f).rgb + inputImage.SampleLevel(linearSampler, uv + dir * 0.5f, 0.0f).rgb);

    // Take wide blend unless it exceeds the range of the neighborhood.
    float lumaWide = dot(rgbWide, SwPostProcess::LUMA);
    float3 result = (lumaWide < lumaMin || lumaWide > lumaMax) ? rgbNarrow : rgbWide;

    outputImage.Store(pixel, float4(result, center.a));
}
