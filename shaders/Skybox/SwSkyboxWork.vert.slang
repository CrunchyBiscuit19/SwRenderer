import SwSkybox;
import SwCommon;

[[vk::push_constant]]
SwSkybox::WorkPC workPc;

SwSkybox::VSOutput main(SwSkybox::VSInput in)
{
    SwSkybox::VSOutput out;

    float4 position = workPc.mWorkVertexBuffer[in.mVertexIndex].mPosition;
    float4x4 viewWithoutTranslation =
        float4x4(float4(workPc.mPerFrameBuffer->mPerspective.mView[0].xyz, 0), float4(workPc.mPerFrameBuffer->mPerspective.mView[1].xyz, 0),
                 float4(workPc.mPerFrameBuffer->mPerspective.mView[2].xyz, 0), float4(0, 0, 0, 1));
    out.mPosition = mul(mul(workPc.mPerFrameBuffer->mPerspective.mProj, viewWithoutTranslation), position);
    out.mUvw = position.xyz;

    return out;
}
