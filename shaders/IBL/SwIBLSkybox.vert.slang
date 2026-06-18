import SwIBL;
import SwCommon;

[[vk::push_constant]]
SwIBL::SkyboxPC skyboxPc;

SwIBL::VSOutput main(SwIBL::VSInput in) {
    SwIBL::VSOutput out;

    float4 position = skyboxPc.mWorkVertexBuffer[in.mVertexIndex].mPosition;
    float4x4 viewWithoutTranslation =
        float4x4(float4(skyboxPc.mPerFrameBuffer->mPerspective.mView[0].xyz, 0), float4(skyboxPc.mPerFrameBuffer->mPerspective.mView[1].xyz, 0),
                 float4(skyboxPc.mPerFrameBuffer->mPerspective.mView[2].xyz, 0), float4(0, 0, 0, 1));
    out.mPosition = mul(mul(skyboxPc.mPerFrameBuffer->mPerspective.mProj, viewWithoutTranslation), position);
    out.mUvw = position.xyz;

    return out;
}
