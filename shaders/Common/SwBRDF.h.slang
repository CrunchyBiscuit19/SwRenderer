module SwBRDF;

namespace SwBRDF {
    public static const float PI = 3.14159265358979323846f;

    // GGX / Trowbridge-Reitz normal distribution.
    float distributionGGX(float NdotH, float roughness) {
        float a = roughness * roughness;
        float a2 = a * a;
        float d = NdotH * NdotH * (a2 - 1.f) + 1.f;
        return a2 / max(PI * d * d, 1e-7f);
    }

    // Schlick-GGX geometry term for a single direction (direct-lighting k).
    float geometrySchlickGGX(float NdotX, float k) {
        return NdotX / (NdotX * (1.f - k) + k);
    }

    // Smith geometry: occlusion from both view and light directions.
    float geometrySmith(float NdotV, float NdotL, float roughness) {
        float r = roughness + 1.f;
        float k = (r * r) / 8.f;
        return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
    }

    // Fresnel-Schlick approximation.
    float3 fresnelSchlick(float cosTheta, float3 F0) {
        return F0 + (1.f - F0) * pow(saturate(1.f - cosTheta), 5.f);
    }

    // Cook-Torrance contribution of one light arriving with the given (already attenuated) radiance.
    // N, V, L are unit vectors in world space; L points from the surface toward the light.
    public float3 cookTorrance(float3 N, float3 V, float3 L, float3 albedo, float metallic, float roughness, float3 radiance) {
        float NdotL = max(dot(N, L), 0.f);
        if (NdotL <= 0.f)
            return float3(0.f);

        float3 H = normalize(V + L);
        float NdotV = max(dot(N, V), 1e-4f);
        float NdotH = max(dot(N, H), 0.f);
        float VdotH = max(dot(V, H), 0.f);

        float3 F0 = lerp(float3(0.04f), albedo, metallic);
        float D = distributionGGX(NdotH, roughness);
        float G = geometrySmith(NdotV, NdotL, roughness);
        float3 F = fresnelSchlick(VdotH, F0);

        float3 specular = (D * G * F) / max(4.f * NdotV * NdotL, 1e-4f);

        // Energy conservation: metals have no diffuse; specular-reflected energy is removed from diffuse.
        float3 kD = (float3(1.f) - F) * (1.f - metallic);
        float3 diffuse = kD * albedo / PI;

        return (diffuse + specular) * radiance * NdotL;
    }
}
