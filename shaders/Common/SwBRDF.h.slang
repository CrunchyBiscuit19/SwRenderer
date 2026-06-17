module SwBRDF;

namespace SwBRDF {
    public static const float PI = 3.1415926535897932384626433832795028841972f;
    public static const float DIELECTRICS_REFLECTANCE = 0.04f;

    // Equirectangular mapping
    public float2 dirToEquirectUv(float3 dir) {
        float3 d = normalize(dir);
        float phi = atan2(d.z, d.x);
        float theta = asin(clamp(d.y, -1.f, 1.f));
        return float2(phi / (2.f * PI) + 0.5f, theta / PI + 0.5f);
    }

    public float3 equirectUvToDir(float2 uv) {
        float phi = (uv.x - 0.5f) * 2.f * PI;
        float theta = (uv.y - 0.5f) * PI;
        float cosTheta = cos(theta);
        return float3(cos(phi) * cosTheta, sin(theta), sin(phi) * cosTheta);
    }

    public float distributionGGX(float NdotH, float roughness) {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH2 = NdotH * NdotH;

        float d = NdotH2 * (a2 - 1.f) + 1.f;
        return a2 / max(PI * d * d, 1e-7f);
    }

    float3 fresnelSchlick(float cosTheta, float3 F0) {
        return F0 + (1.f - F0) * pow(saturate(1.f - cosTheta), 5.f);
    }

    public float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
        float3 r = max(float3(1.f - roughness), F0);
        return F0 + (r - F0) * pow(saturate(1.f - cosTheta), 5.f);
    }

    float geometrySchlickGGX(float NdotX, float k) {
        return NdotX / (NdotX * (1.f - k) + k);
    }

    float geometrySmith(float NdotV, float NdotL, float roughness) {
        float r = roughness + 1.f;
        float k = (r * r) / 8.f;
        return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
    }

    public float3 cookTorrance(float3 N, float3 V, float3 L, float3 albedo, float metallic, float roughness, float3 radiance) {
        float NdotL = max(dot(N, L), 0.f);
        if (NdotL <= 0.f)
            return float3(0.f);

        float3 H = normalize(V + L);
        float NdotV = max(dot(N, V), 1e-4f);
        float NdotH = max(dot(N, H), 0.f);
        float VdotH = max(dot(V, H), 0.f);

        float3 F0 = lerp(DIELECTRICS_REFLECTANCE, albedo, metallic);
        float D = distributionGGX(NdotH, roughness);
        float G = geometrySmith(NdotV, NdotL, roughness);
        float3 F = fresnelSchlick(VdotH, F0);

        float3 kS = F;
        float3 specular = (D * G * F) / max(4.f * NdotV * NdotL, 1e-4f);
        float3 kD = (float3(1.f) - kS) * (1.f - metallic);
        float3 diffuse = albedo / PI;

        return (kD * diffuse + specular) * radiance * NdotL;
        // Radiance for how bright the light is
        // NdotL for scaling the strength of the light by its incidence angle
    }
}
