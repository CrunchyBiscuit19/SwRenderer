module SwLighting;

import SwCommon;
import SwCull;
import SwBRDF;
import SwIBL;

namespace SwLighting {
    public struct ShadowCullPC {
        public Sw::RenderCommand* mLightRcsBuffer;
        public Sw::RenderItem* mRisBuffer;
        public uint mRisCount;
        public uint* mLightDrawRisIndicesBuffer;
        public SwCull::Plane* mFrustumBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
        public Sw::Bounds* mSceneBoundsBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
    };

    public struct ShadowDrawPC {
        public uint* mLightDrawRisIndicesBuffer;
        public Sw::RenderCommand* mLightRcsBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
        public Sw::Vertex* mSceneVertexBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
        public Sw::MaterialConstant* mSceneMaterialConstantsBuffer;
        public uint mLightIndex;  
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mInstanceIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation float mAlphaCutoff;
        public nointerpolation uint mMaterialIndex;
        public nointerpolation uint mModelIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public struct VSOutputOpaque {
        public float4 mPosition : SV_Position;
        public nointerpolation uint mModelIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };

    // Resolve one punctual light's world-space transform from its instance / node-transform indices
    // and return its Cook-Torrance contribution (point: inverse-square + range falloff, spot: cone falloff).
    public float3 accumulatePunctualLight(
        Sw::Light light, float3 worldPos, float3 normal, float3 view, float3 albedo, float metallic, float roughness, Sw::Instance* instances,
        Sw::NodeTransform* nodeTransforms
    ) {
        float4x4 model = mul(instances[light.mInstanceIndex].mTransformMatrix, nodeTransforms[light.mNodeTransformIndex]);
        float3 lightWorldPos = mul(model, float4(0.f, 0.f, 0.f, 1.f)).xyz;
        float3 lightForward = normalize(mul(model, float4(0.f, 0.f, -1.f, 0.f)).xyz);  // glTF: light shines down local -Z

        float3 toLightDir;  // unit vector from the surface toward the light
        float attenuation = 1.f;
        if (light.mType == Sw::LightType::Directional) {
            toLightDir = -lightForward;
        } else {
            float3 toLight = lightWorldPos - worldPos;
            float dist = length(toLight);
            toLightDir = toLight / max(dist, 1e-4f);
            attenuation = 1.f / max(dist * dist, 1e-4f);
            if (light.mRange > 0.f) {
                float rangeFactor = saturate(1.f - pow(dist / light.mRange, 4.f));
                attenuation *= rangeFactor * rangeFactor;
            }
            if (light.mType == Sw::LightType::Spot) {
                float cosAngle = dot(-toLightDir, lightForward);
                attenuation *= smoothstep(light.mOuterCos, light.mInnerCos, cosAngle);
            }
        }

        float3 radiance = light.mColor * light.mIntensity * attenuation;
        return SwBRDF::cookTorrance(normal, view, toLightDir, albedo, metallic, roughness, radiance);
    }

    // Reversed-Z so the bias is added (not subtracted) to push receivers toward lit and kill acne.
    public float shadowFactor(
        uint slot, float3 worldPos, Sw::PerFrameData* perFrame, Texture2D shadowMaps[Sw::MAX_SHADOW_LIGHTS], SamplerComparisonState shadowSampler
    ) {
        float4 clip = mul(perFrame->mLightViewProj[slot], float4(worldPos, 1.f));
        if (clip.w <= 0.f) {
            return 1.f;
        }
        float3 ndc = clip.xyz / clip.w;
        float2 uv = ndc.xy * 0.5f + 0.5f;
        if (any(uv < 0.f) || any(uv > 1.f)) {
            return 1.f;
        }

        const float bias = 0.0015f;
        const float2 texel = 1.f / float2(Sw::SHADOW_MAP_SIZE, Sw::SHADOW_MAP_SIZE);
        float lit = 0.f;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                lit += shadowMaps[slot].SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * texel, ndc.z + bias);
            }
        }
        return lit / 9.f;
    }

    // Full PBR shading: directional sun + image-based ambient (AO-modulated) + the per-frame selected punctual lights.
    public float3 shadeLit(
        float3 albedo, float metallic, float roughness, float ao, float3 worldPos, float3 normal, float3 view, Sw::PerFrameData* perFrame,
        Sw::Light* lights, Sw::Instance* instances, Sw::NodeTransform* nodeTransforms, Sampler2D irradianceMap, Sampler2D prefilterMap,
        Sampler2D brdfLut, float maxPrefilterMip, float iblIntensity, uint iblComponents, Texture2D shadowMaps[Sw::MAX_SHADOW_LIGHTS],
        SamplerComparisonState shadowSampler
    ) {
        Sw::Sunlight sun = perFrame->mSunlight;
        float3 toSunDir = sun.getDirection();
        float3 color = SwBRDF::cookTorrance(normal, view, toSunDir, albedo, metallic, roughness, sun.mColor * sun.mIntensity);
        color += iblIntensity * SwIBL::ambientIBL(albedo, metallic, roughness, ao, normal, view, irradianceMap, prefilterMap, brdfLut, maxPrefilterMip, iblComponents);
        for (uint i = 0; i < perFrame->mActiveLightCount; i++) {
            Sw::Light light = lights[perFrame->mActiveLightIndices[i]];
            float3 contribution = accumulatePunctualLight(light, worldPos, normal, view, albedo, metallic, roughness, instances, nodeTransforms);
            // Only spotlights cast shadows for now; their map lives in slot i, parallel to the active-light records.
            if (light.mType == Sw::LightType::Spot) {
                contribution *= shadowFactor(i, worldPos, perFrame, shadowMaps, shadowSampler);
            }
            color += contribution;
        }
        return color;
    }
}