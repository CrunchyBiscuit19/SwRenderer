import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

// Shared shading for the opaque and masked geometry passes. `masked` is a compile-time
// constant at each entry point below, so the opaque path folds away the discard entirely
// (no OpKill -> [earlydepthstencil] stays valid and early-Z is preserved), while the masked
// path keeps the alpha-cutout discard.
float4 shade(SwGeometry::FSInput in, bool masked) {
    float3 normal = normalize(in.mNormal);
    float3 sunlightDir = normalize(float3(1.f, 0.f, 0.f));
    float sunlightPower = 1.f;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.mMaterialIndex * SwGeometry::NUM_PBR_IMAGES + 0)].Sample(in.mUv);
    if (masked && in.mColor.a * diffuseColor.a < in.mAlphaCutoff)
        discard;

    float4 color = in.mColor * diffuseColor;
    float finalAlpha = color.a;
    float diffuse = max(dot(normal, sunlightDir), 0.f);
    float4 sunlight = float4(float3(1.f), 1.f) * sunlightPower;

    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * float4(float3(1.f), 1.f);

    float4 finalColor = lightColor + ambient;
    finalColor.a = finalAlpha;

    return finalColor;
}

// Opaque geometry: no discard, so early depth test/write is safe.
[shader("fragment")]
[earlydepthstencil]
SwGeometry::FSOutputOpaque mainOpaque(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputOpaque output;
    output.mColor = shade(in, false);
    return output;
}

// Masked geometry: alpha-cutout discard, so depth must not be written early.
[shader("fragment")]
SwGeometry::FSOutputOpaque mainMasked(SwGeometry::FSInput in) : SV_Target {
    SwGeometry::FSOutputOpaque output;
    output.mColor = shade(in, true);
    return output;
}

// TODO un-hardcode sunlight data
