module SwFXAA;

__exported import SwCommon;

namespace SwFXAA {
    public struct WorkPC {
        public float2 mInverseScreenSize;
    };

    public struct CSInput {
        public uint3 mDispatchThreadID : SV_DispatchThreadID;
    };
}
