import SwPick;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

uint2 shade(SwPick::FSInput in, bool masked) {
    if (masked) {
        float4 diffuseColor = materialResources[NonUniformResourceIndex(in.mMaterialIndex * Sw::NUM_PBR_IMAGES + 0)].Sample(in.mUv);
        if (in.mColor.a * diffuseColor.a < in.mAlphaCutoff)
            discard;
    }
    return uint2(in.mModelIndex + 1, in.mInstanceIndex + 1);
}

[shader("fragment")]
[earlydepthstencil]
uint2 mainOpaque(SwPick::FSInput in) : SV_Target {
    return shade(in, false);
}

[shader("fragment")]
uint2 mainMasked(SwPick::FSInput in) : SV_Target {
    return shade(in, true);
}
