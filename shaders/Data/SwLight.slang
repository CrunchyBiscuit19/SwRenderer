module SwLight;

__exported import Data.SwFrustum;

namespace SwLight {
    public enum Type : uint {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    public struct Data {
        public float3 mColor;
        public float mIntensity;
        public Type mType;
        public float mRange;               // Less than or equal to 0 = infinite
        public float mInnerCos;            // spot inner cone, cos
        public float mOuterCos;            // spot outer cone, cos
        public uint mNodeTransformIndex;   // into the scene node-transform buffer
        public uint mInstanceIndex;        // into the scene instance buffer
        public float3 mBasePosition;       // light position in node-local space
        public float3 mBaseDirection;      // light forward in node-local space (glTF local -Z)
        public uint2 mShadowMapSlot;
        public SwFrustum mFrustum;         // world-space frustum, rebuilt by the LightsFrustum pass on edit
    };
}
