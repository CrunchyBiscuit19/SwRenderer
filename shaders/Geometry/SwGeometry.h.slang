module SwGeometry;

import SwCommon;
import SwBRDF;

namespace SwGeometry {
    public static const uint BASE_TEXTURE_OFFSET = 0;
    public static const uint METALLIC_ROUGHNESS_TEXTURE_OFFSET = 1;
    public static const uint NORMAL_TEXTURE_OFFSET = 2;
    public static const uint OCCLUSION_TEXTURE_OFFSET = 3;
    public static const uint EMISSIVE_TEXTURE_OFFSET = 4;

    public struct WorkPC {
        public Sw::Vertex* mSceneVertexBuffer;
        public Sw::MaterialConstant* mSceneMaterialConstantsBuffer;
        public Sw::NodeTransform* mSceneNodeTransformsBuffer;
        public Sw::Instance* mSceneInstancesBuffer;
        public uint* mSceneDrawRisIndicesBuffer;
        public Sw::RenderCommand *mDrawRcsBuffer;
        public Sw::PerFrameData *mPerFrameBuffer;
        public Sw::Light* mSceneLightsBuffer;
        public uint mLightCount;
        public float mMaxPrefilterMip;   // highest mip index of the specular prefilter chain
        public float mIblIntensity;      // scales the image-based ambient term (GUI-controlled)
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

    // Image-based ambient: split-sum diffuse irradiance + specular prefilter, modulated by AO.
    // Replaces the flat constant ambient now that the skybox drives a real environment.
    float3 ambientIBL(
        float3 albedo, float metallic, float roughness, float ao, float3 normal, float3 view, Sampler2D irradianceMap, Sampler2D prefilterMap,
        Sampler2D brdfLut, float maxPrefilterMip
    ) {
        float NdotV = max(dot(normal, view), 1e-4f);
        float3 R = reflect(-view, normal);
        float3 F0 = lerp(float3(0.04f), albedo, metallic);
        float3 F = SwBRDF::fresnelSchlickRoughness(NdotV, F0, roughness);
        float3 kD = (1.f - F) * (1.f - metallic);

        float3 irradiance = irradianceMap.SampleLevel(SwBRDF::dirToEquirectUv(normal), 0.f).rgb;
        float3 diffuseIBL = irradiance * albedo;

        float prefilterLod = roughness * maxPrefilterMip;
        float3 prefiltered = prefilterMap.SampleLevel(SwBRDF::dirToEquirectUv(R), prefilterLod).rgb;
        float2 envBrdf = brdfLut.SampleLevel(float2(NdotV, roughness), 0.f).rg;
        float3 specularIBL = prefiltered * (F0 * envBrdf.x + envBrdf.y);

        return (kD * diffuseIBL + specularIBL) * ao;
    }

    // Full PBR shading: directional sun + image-based ambient (AO-modulated) + every punctual light in the scene buffer.
    public float3 shadeLit(
        float3 albedo, float metallic, float roughness, float ao, float3 worldPos, float3 normal, float3 view, Sw::Sunlight sun, Sw::Light* lights,
        uint lightCount, Sw::Instance* instances, Sw::NodeTransform* nodeTransforms, Sampler2D irradianceMap, Sampler2D prefilterMap, Sampler2D brdfLut,
        float maxPrefilterMip, float iblIntensity
    ) {
        float3 toSunDir = sun.getDirection();  // unit vector from the surface toward the sun
        float3 color = SwBRDF::cookTorrance(normal, view, toSunDir, albedo, metallic, roughness, sun.mColor * sun.mIntensity);
        color += iblIntensity * ambientIBL(albedo, metallic, roughness, ao, normal, view, irradianceMap, prefilterMap, brdfLut, maxPrefilterMip);
        for (uint i = 0; i < lightCount; i++) {
            color += accumulatePunctualLight(lights[i], worldPos, normal, view, albedo, metallic, roughness, instances, nodeTransforms);
        }
        return color;
    }

    // Build a world-space TBN and perturb the geometric normal by a tangent-space normal-map sample.
    public float3 applyNormalMap(float3 normalWS, float4 tangentWS, float3 sampledNormal, float normalScale) {
        float3 n = normalize(normalWS);
        float3 t = normalize(tangentWS.xyz - n * dot(n, tangentWS.xyz));  // Gram-Schmidt re-orthonormalise
        float3 b = cross(n, t) * tangentWS.w;                            // handedness from glTF tangent.w
        float3 ts = sampledNormal * 2.f - 1.f;                           // unpack [0,1] -> [-1,1]
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

