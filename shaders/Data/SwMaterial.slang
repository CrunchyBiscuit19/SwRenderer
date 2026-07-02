module SwMaterial;

namespace SwMaterial {
    public static const uint NUM_PBR_IMAGES = 5;

    public struct Constant {
        public float4 mBaseFactor;
        public float4 mEmissiveFactor;
        public float2 mMetallicRoughnessFactor;
        public float mNormalScale;
        public float mOcclusionStrength;
        public float mAlphaCutoff;
    };
}
