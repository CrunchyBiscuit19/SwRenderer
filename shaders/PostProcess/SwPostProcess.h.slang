module SwPostProcess;

namespace SwPostProcess {
    public static const float3 LUMA = float3(0.299f, 0.587f, 0.114f);
    public static const float EDGE_THRESHOLD = 1.0f / 8.0f;      // Relative contrast needed to anti-alias.
    public static const float EDGE_THRESHOLD_MIN = 1.0f / 32.0f;  // Absolute contrast floor (kills dark-region noise).
    public static const float REDUCE_MUL = 1.0f / 8.0f;
    public static const float REDUCE_MIN = 1.0f / 128.0f;
    public static const float SPAN_MAX = 8.0f;

    public struct FxaaPC {
        public float2 mInverseScreenSize;
    };

    public struct TonemapPC {
        public float mExposure;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}
