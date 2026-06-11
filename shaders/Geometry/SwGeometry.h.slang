module SwGeometry;

import SwCommon;

namespace SwGeometry {
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
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation uint mMaterialIndex;
    };

    public typedef VSOutput FSInput;

    // Resolve one punctual light's world-space transform from its instance / node-transform indices
    // and return its diffuse contribution (point: inverse-square + range falloff, spot: cone falloff).
    public float3 accumulatePunctualLight(
        Sw::Light light, float3 worldPos, float3 normal, float3 albedo, Sw::Instance* instances, Sw::NodeTransform* nodeTransforms
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

        float diffuse = max(dot(normal, toLightDir), 0.f);
        return albedo * diffuse * light.mColor * light.mIntensity * attenuation;
    }

    // Full diffuse shading: directional sun + ambient + every punctual light in the scene buffer.
    public float3 shadeLit(
        float3 albedo, float3 worldPos, float3 normal, Sw::Sunlight sun, Sw::Light* lights, uint lightCount, Sw::Instance* instances,
        Sw::NodeTransform* nodeTransforms
    ) {
        float3 toSunDir = normalize(-sun.mDirection);  
        float3 color = albedo * max(dot(normal, toSunDir), 0.f) * sun.mColor * sun.mIntensity;
        color += albedo * sun.mAmbient;
        for (uint i = 0; i < lightCount; i++) {
            color += accumulatePunctualLight(lights[i], worldPos, normal, albedo, instances, nodeTransforms);
        }
        return color;
    }

    public struct FSOutputOpaque {
        public float4 mColor : SV_Target0;
    };

    public struct FSOutputTransparent {
        public float4 mAccum : SV_Target0;
        public float4 mRvl : SV_Target1;
    };
};

