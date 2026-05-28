module SwSkybox;

import SwCommon;

namespace SwSkybox {
    public struct Vertex {
        public float4 mPosition;
    };
    public struct WorkPC {
        public SwSkybox::Vertex* mWorkVertexBuffer;
        public SwPerFrameData* mPerFrameBuffer;
    };
    public struct VSInput {
        public uint mVertexIndex : SV_VulkanVertexID;
    };
    public struct VSOutput {
        public float4 mPosition : SV_Position;
        public float3 mUvw : TEXCOORD0;
    };
    public typedef VSOutput FSInput;
};

