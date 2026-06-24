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
            toLightDir = lightWorldPos - worldPos;
            toLightDir /= max(length(toLightDir), 1e-4f);
            attenuation = 1.f / max(length(toLightDir) * length(toLightDir), 1e-4f);
            if (light.mRange > 0.f) {
                float rangeFactor = saturate(1.f - pow(length(toLightDir) / light.mRange, 4.f));
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

    public static const uint POISSON_COUNT = 16;
    public static const int BLUR_RANGE = 1;
    public static const int SAMPLES_COUNT = (BLUR_RANGE * 2 + 1) * (BLUR_RANGE * 2 + 1);
    public static const float PCF_SCALE = 0.5;
    public static const float SHADOW_AMBIENT = 0.15;
    public static const float POISSON_RADIUS = 5000.0; // Smaller means blurrier
    public static const float2 POISSON_DISK[POISSON_COUNT] = float2[](
	    float2(-0.94201624, -0.39906216),
	    float2(0.94558609, -0.76890725),
	    float2(-0.094184101, -0.92938870),
	    float2(0.34495938, 0.29387760),
	    float2(-0.91588581, 0.45771432),
	    float2(-0.81544232, -0.87912464),
	    float2(-0.38277543, 0.27676845),
	    float2(0.97484398, 0.75648379),
	    float2(0.44323325, -0.97511554),
	    float2(0.53742981, -0.47373420),
	    float2(-0.26496911, -0.41893023),
	    float2(0.79197514, 0.19090188),
	    float2(-0.24188840, 0.99706507),
	    float2(-0.81409955, 0.91437590),
	    float2(0.19984126, 0.78641367),
	    float2(0.14383161, -0.14100790)
    );

    // Returns a pseudo random number based on a float3 and an int.
    float Rand(float3 seed, int i) {
	    float4 seed4 = float4(seed, i);
	    float dotValue = dot(seed4, float4(12.9898, 78.233, 45.164, 94.673));
	    return fract(sin(dotValue) * 43758.5453);
    }

    float2 GetPoissonDiskCoord(float2 projCoords, int i, float radius) {
	    int index = int(float(POISSON_COUNT) * Rand(projCoords.xyy, i)) % POISSON_COUNT;
	    return projCoords.xy + POISSON_DISK[index] / radius;
    }

    // Reversed-Z so the bias is added (not subtracted) to kill acne.
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
        const float radius = ndc.z + bias;  // reversed-Z, so bias is added to push receivers toward lit
        const float2 texel = 1.f / float2(Sw::SHADOW_MAP_SIZE, Sw::SHADOW_MAP_SIZE);
        float lit = 0.f;
        int count = 0;
        for (int y = -BLUR_RANGE; y <= BLUR_RANGE; y++) {
            for (int x = -BLUR_RANGE; x <= BLUR_RANGE; x++) {
                float2 off = float2(texel.x * x, texel.y * y);
                float2 coord = GetPoissonDiskCoord(uv + off, count, POISSON_RADIUS);
                lit += shadowMaps[slot].SampleCmpLevelZero(shadowSampler, coord, radius).r;
                count++;
            }
        }
        float shadow = lit / count;  // 0 = fully occluded, 1 = fully lit
        return lerp(SwLighting::SHADOW_AMBIENT, 1.0, shadow);
    }
}