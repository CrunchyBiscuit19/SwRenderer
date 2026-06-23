module SwGeometry;

import SwCommon;
import SwBRDF;

namespace SwGeometry {
    public static const uint BASE_TEXTURE_OFFSET = 0;
    public static const uint METALLIC_ROUGHNESS_TEXTURE_OFFSET = 1;
    public static const uint NORMAL_TEXTURE_OFFSET = 2;
    public static const uint OCCLUSION_TEXTURE_OFFSET = 3;
    public static const uint EMISSIVE_TEXTURE_OFFSET = 4;

    // IBL ambient component mask (GUI-controlled): diffuse irradiance and/or specular prefilter.
    public static const uint IBL_DIFFUSE = 1u;
    public static const uint IBL_SPECULAR = 2u;

    public struct WorkPC {
        public Sw::Vertex* mSceneVertexBuffer;
        public Sw::MaterialConstant* mSceneMaterialConstantsBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
        public uint* mSceneDrawRisIndicesBuffer;
        public Sw::RenderCommand* mDrawRcsBuffer;
        public Sw::PerFrameData* mPerFrameBuffer;
        public Sw::Light* mSceneLightsBuffer;
        public float mMaxPrefilterMip;   // highest mip index of the specular prefilter chain
        public float mIblIntensity;      // scales the image-based ambient term (GUI-controlled)
        public uint mIblComponents;      // IBL_DIFFUSE / IBL_SPECULAR bit mask (GUI-controlled)
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mInstanceIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float3 mWorldPos : POSITION0;
        public float3 mNormal : NORMAL;
        public float4 mTangent : TANGENT; // w = handedness
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation uint mMaterialIndex;
    };

    public typedef VSOutput FSInput;

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

    // Spotlight shadow factor for the light occupying shadow slot 1.0 fully lit, 0.0 fully shadowed.
    // Reversed-Z so the bias is added (not subtracted) to push receivers toward lit and kill acne.
    public float spotShadowFactor(
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

    // Image-based ambient: split-sum diffuse irradiance + specular prefilter, modulated by AO.
    // Replaces the flat constant ambient now that the skybox drives a real environment.
    float3 ambientIBL(
        float3 albedo, float metallic, float roughness, float ao, float3 normal, float3 view, Sampler2D irradianceMap, Sampler2D prefilterMap,
        Sampler2D brdfLut, float maxPrefilterMip, uint components
    ) {
        float NdotV = max(dot(normal, view), 1e-4f);
        float3 F0 = lerp(float3(0.04f), albedo, metallic);

        float3 ambient = float3(0.f);
        if ((components & IBL_DIFFUSE) != 0u) {
            // Weight by the normal-incidence Fresnel F0 (not the view-dependent F) so the diffuse fill
            // stays camera-invariant while keeping a sensible diffuse/specular energy split.
            float3 kD = (1.f - F0) * (1.f - metallic);
            float3 irradiance = irradianceMap.SampleLevel(SwBRDF::dirToEquirectUv(normal), 0.f).rgb;
            ambient += kD * irradiance * albedo;
        }
        if ((components & IBL_SPECULAR) != 0u) {
            float3 R = reflect(-view, normal);
            float prefilterLod = roughness * maxPrefilterMip;
            float3 prefiltered = prefilterMap.SampleLevel(SwBRDF::dirToEquirectUv(R), prefilterLod).rgb;
            float2 envBrdf = brdfLut.SampleLevel(float2(NdotV, roughness), 0.f).rg;
            ambient += prefiltered * (F0 * envBrdf.x + envBrdf.y);
        }

        return ambient * ao;
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
        color += iblIntensity * ambientIBL(albedo, metallic, roughness, ao, normal, view, irradianceMap, prefilterMap, brdfLut, maxPrefilterMip, iblComponents);
        for (uint i = 0; i < perFrame->mActiveLightCount; i++) {
            Sw::Light light = lights[perFrame->mActiveLightIndices[i]];
            float3 contribution = accumulatePunctualLight(light, worldPos, normal, view, albedo, metallic, roughness, instances, nodeTransforms);
            // Only spotlights cast shadows for now; their map lives in slot i, parallel to the active-light records.
            if (light.mType == Sw::LightType::Spot) {
                contribution *= spotShadowFactor(i, worldPos, perFrame, shadowMaps, shadowSampler);
            }
            color += contribution;
        }
        return color;
    }

    // Build a world-space TBN, robust to missing / degenerate vertex tangents.
    public float3 applyNormalMap(float3 normalWS, float4 tangentWS, float3 sampledNormal, float normalScale) {
        float3 n = normalize(normalWS);

        // Gram-Schmidt re-orthonormalise the tangent against the normal. 
        // If the supplied tangent is zero, NaN, or (nearly) parallel to the normal, the residual collapses toward zero. Normalising this is bad (the "acne"). 
        // Detect that and derive a stable basis from the normal instead. The !(len > eps) test also treats a NaN residual as degenerate.
        float3 t = tangentWS.xyz - n * dot(n, tangentWS.xyz);
        float tLen = length(t);
        float handedness = tangentWS.w;
        if (!(tLen > 1e-4f)) {
            float3 up = abs(n.y) < 0.999f ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);
            t = normalize(cross(up, n));
            handedness = 1.f;
        } else {
            t = t / tLen;
        }
        float3 b = cross(n, t) * handedness; // handedness from glTF tangent.w
        float3 ts = sampledNormal * 2.f - 1.f; // unpack [0,1] -> [-1,1]
        ts.xy *= normalScale;
        return normalize(t * ts.x + b * ts.y + n * ts.z);
    }

    public struct FSOutputOpaque {
        public float4 mColor : SV_Target0;
    };

    public struct FSOutputTransparent {
        public float4 mAccum : SV_Target0;
        public float4 mRvl : SV_Target1;
    };
};

