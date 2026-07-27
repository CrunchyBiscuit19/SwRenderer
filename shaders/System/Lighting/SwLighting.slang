module SwLighting;

__exported import Data.SwBatch;
__exported import Data.SwCamera;
__exported import Data.SwFrame;
__exported import Data.SwFrustum;
__exported import Data.SwInstance;
__exported import Data.SwLight;
__exported import Data.SwMaterial;
__exported import Data.SwMesh;
__exported import Data.SwNode;
import System.Cull.SwCull;
import System.BRDF.SwBRDF;
import System.IBL.SwIBL;

namespace SwLighting {
    public static const uint3 CLUSTERS_DIMENSIONS = uint3(16,9,24);
    public static const uint NUM_CLUSTERS = CLUSTERS_DIMENSIONS.x * CLUSTERS_DIMENSIONS.y * CLUSTERS_DIMENSIONS.z;

    public struct Cluster {
        public float3 mMin;
        public float3 mMax;
    };

    public struct ResetPC {
        public SwRenderCommand* mShadowsRcsBuffer;
        public uint mShadowsRcsLimit;
    };

    public struct ClustersBuildPC {
        public SwFrame::Data* mFrameBuffer;
        public Cluster* mClustersBuffer;
        public float4x4 mInvProj;
        public uint2 mTargetSize;
    };

    public struct ClustersMarkActivePC {
        public SwFrame::Data* mFrameBuffer;
        public Cluster* mClustersBuffer;
        public bool* mClustersActiveBooleansBuffer;
    };

    public struct ClustersCompactActivePC {
        public uint* mClustersActiveIndicesBuffer;
        public bool* mClustersActiveBooleansBuffer;
    };

    public struct LightsCullPC {
        public SwFrame::Data* mFrameBuffer;
        public SwLight::Data* mLightsBuffer;
        public SwNode::Transform* mNodeTransformsBuffer;
        public SwInstance::Data* mInstancesBuffer;
        public uint* mLitIndicesBuffer;
        public uint* mShadowMapSlotsCount;
        public uint mLightsLimit;
    };

    public struct ClustersLightCalcOffsetPC {
        public SwFrame::Data* mFrameBuffer;
        public SwLight::Data* mLightsBuffer;
        public SwNode::Transform* mNodeTransformsBuffer;
        public SwInstance::Data* mInstancesBuffer;
        public Cluster* mClustersBuffer;
        public uint* mClustersActiveIndicesBuffer; // 1st 4 bytes hold the active cluster count
        public uint* mLitIndicesBuffer; // 1st 4 bytes hold the lit light count
        public uint* mClustersLightCounts;
    };

    public struct ClustersLightPrefixSumOffsetPC {
        public uint* mClustersLightCounts;
        public uint* mClustersLightOffsetsBuffer;
    };

    public struct ClustersLightSelectPC {
        public SwFrame::Data* mFrameBuffer;
        public SwLight::Data* mLightsBuffer;
        public SwNode::Transform* mNodeTransformsBuffer;
        public SwInstance::Data* mInstancesBuffer;
        public Cluster* mClustersBuffer;
        public uint* mClustersActiveIndicesBuffer; // 1st 4 bytes hold the active cluster count
        public uint* mLitIndicesBuffer; // 1st 4 bytes hold the lit light count
        public uint* mClustersLightIndicesBuffer;
        public uint* mClustersLightCounts;
        public uint* mClustersLightOffsetsBuffer;
        public uint* mClustersLightWriteCursorsBuffer;
    };

    public struct ShadowsCullPC {
        public SwRenderCommand* mShadowsRcsBuffer;
        public SwRenderItem* mShadowsRisBuffer;
        public uint* mShadowsRisIndicesBuffer;
        public SwLight::Data* mLightsBuffer;
        public SwBounds* mBoundsBuffer;
        public SwNode::Transform* mNodeTransformsBuffer;
        public SwInstance::Data* mInstancesBuffer;
        public uint mShadowsRisLimit;
    };

    public struct ShadowDrawPC {
        public SwRenderCommand* mShadowsRcsBuffer;
        public uint* mShadowsRisIndicesBuffer;
        public SwLight::Data* mLightsBuffer;
        public SwVertex* mVertexBuffer;
        public SwNode::Transform* mNodeTransformsBuffer;
        public SwInstance::Data* mInstancesBuffer;
        public SwMaterial::Constant* mMaterialConstantsBuffer;
    };

    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mRenderItemIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float2 mUv : TEXCOORD0;
        public float4 mColor : COLOR0;
        public nointerpolation float mAlphaCutoff;
        public nointerpolation uint mMaterialIndex;
        public nointerpolation uint mAssetIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public struct VSOutputOpaque {
        public float4 mPosition : SV_Position;
        public nointerpolation uint mAssetIndex;
        public nointerpolation uint mInstanceIndex;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };

    public uint getDepthSlice(float ndcZ, float2 planes) {
        const uint numSlices = CLUSTERS_DIMENSIONS.z;
        const float viewZ = planes.y * planes.x / (planes.x + ndcZ * (planes.y - planes.x));
        const float numerator = numSlices * (log(viewZ) - log(planes.x));
        const float denominator = log(planes.y / planes.x);

        return min(uint(floor(numerator / denominator)), numSlices - 1);
    }

    public uint getClusterIndex(float2 screenPixel, float ndcZ, uint2 targetSize, float2 planes) {
        const uint tileSize = targetSize.x / CLUSTERS_DIMENSIONS.x;
        const uint2 tile = min(uint2(screenPixel / tileSize), CLUSTERS_DIMENSIONS.xy - 1);
        const uint3 cluster = uint3(tile, getDepthSlice(ndcZ, planes));

        return cluster.x + CLUSTERS_DIMENSIONS.x * cluster.y + (CLUSTERS_DIMENSIONS.x * CLUSTERS_DIMENSIONS.y) * cluster.z;
    }
}

namespace SwLighting::Collision {
    public bool lightBoundingSphere(
        SwLight::Data light, SwInstance::Data* instances, SwNode::Transform* nodeTransforms, out float3 center, out float radius
    ) {
        center = float3(0.f);
        radius = 0.f;
        if (light.mType == SwLight::Type::Directional || light.mRange <= 0.f) {
            return false;
        }

        float4x4 model = mul(instances[light.mInstanceIndex].mTransformMatrix, nodeTransforms[light.mNodeTransformIndex]);
        center = mul(model, float4(light.mBasePosition, 1.f)).xyz;
        float3 forward = normalize(mul(model, float4(light.mBaseDirection, 0.f)).xyz);
        radius = light.mRange;

        if (light.mType == SwLight::Type::Spot) {
            float cosHalf = light.mOuterCos;
            if (cosHalf * cosHalf >= 0.5f) {
                radius = light.mRange / (2.f * cosHalf * cosHalf);
                center += forward * radius;
            } else {
                float sinHalf = sqrt(1.f - cosHalf * cosHalf);
                radius = light.mRange * sinHalf / cosHalf;
                center += forward * light.mRange;
            }
        }
        return true;
    }

    public bool sphereIntersectsAABB(float3 center, float radius, float3 boxMin, float3 boxMax) {
        float3 closest = clamp(center, boxMin, boxMax);
        float3 delta = center - closest;
        return dot(delta, delta) <= radius * radius;
    }

    public bool lightTouchesCluster(
        SwLight::Data light, Cluster cluster, float4x4 view, SwInstance::Data* instances, SwNode::Transform* nodeTransforms
    ) {
        float3 worldCenter;
        float radius;
        if (!lightBoundingSphere(light, instances, nodeTransforms, worldCenter, radius)) {
            return true;
        }
        float3 viewCenter = mul(view, float4(worldCenter, 1.f)).xyz;
        return sphereIntersectsAABB(viewCenter, radius, cluster.mMin, cluster.mMax);
    }
}

namespace SwLighting::Calculation {
    public struct SurfacePoint {
        public float3 mAlbedo;
        public float mMetallic;
        public float mRoughness;
        public float mAo;
        public float3 mWorldPos;
        public float3 mNormal;
        public float3 mView;
    };

    public float3 accumulatePunctualLight(SwLight::Data light, SurfacePoint surface, SwInstance::Data* instances, SwNode::Transform* nodeTransforms) {
        float4x4 model = mul(instances[light.mInstanceIndex].mTransformMatrix, nodeTransforms[light.mNodeTransformIndex]);
        float3 lightWorldPos = mul(model, float4(light.mBasePosition, 1.f)).xyz;
        float3 lightForward = normalize(mul(model, float4(light.mBaseDirection, 0.f)).xyz);

        float3 toLightDir;
        float attenuation = 1.f;
        if (light.mType == SwLight::Type::Directional) {
            toLightDir = -lightForward;
        } else {
            float3 toLight = lightWorldPos - surface.mWorldPos;
            float dist = length(toLight);
            toLightDir = toLight / max(dist, 1e-4f);
            attenuation = 1.f / max(dist * dist, 1e-4f);
            if (light.mRange > 0.f) {
                float rangeFactor = saturate(1.f - pow(dist / light.mRange, 4.f));
                attenuation *= rangeFactor * rangeFactor;
            }
            if (light.mType == SwLight::Type::Spot) {
                float cosAngle = dot(-toLightDir, lightForward);
                attenuation *= smoothstep(light.mOuterCos, light.mInnerCos, cosAngle);
            }
        }

        float3 radiance = light.mColor * light.mIntensity * attenuation;
        return SwBRDF::cookTorrance(surface.mNormal, surface.mView, toLightDir, surface.mAlbedo, surface.mMetallic, surface.mRoughness, radiance);
    }

    public float3 shadeLit(
        SurfacePoint surface, uint* clustersLightIndices, uint clusterLightCount, uint clusterLightOffset, SwLight::Data* lights,
        SwInstance::Data* instances, SwNode::Transform* nodeTransforms, SwIBL::Params ibl, SwLighting::Shadow::Params shadows
    ) {
        float3 color = ibl.mIntensity * SwIBL::ambientIBL(
            surface.mAlbedo, surface.mMetallic, surface.mRoughness, surface.mAo, surface.mNormal, surface.mView, ibl
        );
        for (uint i = 0; i < clusterLightCount; i++) {
            SwLight::Data light = lights[clustersLightIndices[clusterLightOffset + i]];
            float3 contribution = accumulatePunctualLight(light, surface, instances, nodeTransforms);
            switch (light.mType) {
                case SwLight::Type::Directional:
                    contribution *= SwLighting::Shadow::shadowsFactor2D(i, surface.mWorldPos, shadows.mDirectionalMaps, shadows.mSampler);
                    break;
                case SwLight::Type::Spot:
                    contribution *= SwLighting::Shadow::shadowsFactor2D(i, surface.mWorldPos, shadows.mSpotMaps, shadows.mSampler);
                    break;
                case SwLight::Type::Point:
                    contribution *= SwLighting::Shadow::shadowsFactorCube(i, light, surface.mWorldPos, shadows.mPointMaps, shadows.mSampler);
                    break;
            }
            color += contribution;
        }
        return color;
    }
}

namespace SwLighting::Shadow {
    public static const uint SHADOWS_MAP_SIZE = 1024;
    public static const uint MAX_DIRECTIONAL_SHADOW_MAPS = 4;
    public static const uint MAX_SPOT_SHADOW_MAPS = 16;
    public static const uint MAX_POINT_SHADOW_MAPS = 4;
    public static const uint MAX_NUM_SHADOW_CASTERS = MAX_DIRECTIONAL_SHADOW_MAPS + MAX_SPOT_SHADOW_MAPS + MAX_POINT_SHADOW_MAPS;
    public static const float SHADOWS_SPOT_NEAR = 0.05f;
    public static const float SHADOWS_SPOT_DEFAULT_RANGE = 60.f;

    public struct Params {
        public Texture2D mDirectionalMaps[MAX_DIRECTIONAL_SHADOW_MAPS];
        public Texture2D mSpotMaps[MAX_SPOT_SHADOW_MAPS];
        public TextureCube mPointMaps[MAX_POINT_SHADOW_MAPS];
        public SamplerComparisonState mSampler;
    };

    public static const uint POISSON_COUNT = 16;
    public static const int BLUR_RANGE = 1;
    public static const int SAMPLES_COUNT = (BLUR_RANGE * 2 + 1) * (BLUR_RANGE * 2 + 1);
    public static const float PCF_SCALE = 0.5;
    public static const float SHADOWS_AMBIENT = 0.15;
    public static const float POISSON_RADIUS = 5000.0;  // Smaller means blurrier
    public static const float2 POISSON_DISK[POISSON_COUNT] = float2[](
        float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760),
        float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464), float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
        float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420), float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
        float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
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

    public float shadowsFactor2D<let N : int>(
        uint slot, float3 worldPos, Texture2D shadowMaps[N], SamplerComparisonState shadowsSampler
    ) {
        const bool shadows2DEnabled = false;  // TODO: re-enable once the per-light view-projection is rebuilt
        if (!shadows2DEnabled) {
            return 1.f;
        }

        float4x4 lightViewProj = float4x4(1.f);  // TODO: source the rebuilt per-light view-projection
        float4 clip = mul(lightViewProj, float4(worldPos, 1.f));
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
        const float2 texel = 1.f / float2(SHADOWS_MAP_SIZE, SHADOWS_MAP_SIZE);
        float lit = 0.f;
        int count = 0;
        for (int y = -BLUR_RANGE; y <= BLUR_RANGE; y++) {
            for (int x = -BLUR_RANGE; x <= BLUR_RANGE; x++) {
                float2 off = float2(texel.x * x, texel.y * y);
                float2 coord = GetPoissonDiskCoord(uv + off, count, POISSON_RADIUS);
                lit += shadowMaps[slot].SampleCmpLevelZero(shadowsSampler, coord, radius).r;
                count++;
            }
        }
        float shadow = lit / count;  // 0 = fully occluded, 1 = fully lit
        return lerp(SHADOWS_AMBIENT, 1.0, shadow);
    }

    public float shadowsFactorCube<let N : int>(
        uint slot, SwLight::Data light, float3 worldPos, TextureCube shadowMaps[N], SamplerComparisonState shadowsSampler
    ) {
        return 1.f; // TODO actually implement it
    }
}

