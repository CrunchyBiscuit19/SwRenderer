module SwPostProcess;

namespace SwPostProcess {
    public static const float3 LUMA = float3(0.299f, 0.587f, 0.114f);
    public static const float EDGE_THRESHOLD = 1.0f / 8.0f;      // Relative contrast needed to anti-alias.
    public static const float EDGE_THRESHOLD_MIN = 1.0f / 32.0f;  // Absolute contrast floor (kills dark-region noise).
    public static const float REDUCE_MUL = 1.0f / 8.0f;
    public static const float REDUCE_MIN = 1.0f / 128.0f;
    public static const float SPAN_MAX = 8.0f;

    namespace Fxaa {
        public struct WorkPC {
            public float2 mInverseScreenSize;
        };
    }

    namespace Tonemap {
        public struct WorkPC {
            public float mExposure;
        };

        // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
        public static const float3x3 ACES_INPUT_MATRIX = {
            {0.59719, 0.35458, 0.04823},
            {0.07600, 0.90834, 0.01566},
            {0.02840, 0.13383, 0.83777}
        };

        // ODT_SAT => XYZ => D60_2_D65 => sRGB
        public static const float3x3 ACES_OUTPUT_MATRIX = {
            { 1.60475, -0.53108, -0.07367},
            {-0.10208,  1.10813, -0.00605},
            {-0.00327, -0.07276,  1.07602}
        };

        public float3 RRTAndODTFit(float3 v) {
            float3 a = v * (v + 0.0245786f) - 0.000090537f;
            float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
            return a / b;
        }

        public float3 acesTonemap(float3 hdr) {
            float3 ldr = mul(ACES_INPUT_MATRIX, hdr);
            ldr = RRTAndODTFit(ldr);
            ldr = mul(ACES_OUTPUT_MATRIX, ldr);
            return saturate(ldr);
        }

    }

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}
