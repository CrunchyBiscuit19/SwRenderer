module SwWBOIT;

namespace SwWBOIT {
    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
        public uint mInstanceIndex : SV_VulkanInstanceID;
        public uint mDrawIndex : SV_DrawIndex;
    };

    public struct VSOutput {
        public float4 mPosition : SV_Position;
    };

    public typedef VSOutput FSInput;

    public struct FSOutput {
        public float4 mColor : SV_Target0;
    };
}
