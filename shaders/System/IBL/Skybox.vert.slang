import System.IBL.SwIBL;

[[vk::push_constant]]
SwIBL::SkyboxPC skyboxPc;

SwIBL::VSOutput main(SwIBL::VSInput in) {
    SwIBL::VSOutput out;

    float4 position = skyboxPc.mSkyboxVertexBuffer[in.mVertexIndex].mPosition;
    float4x4 viewWithoutTranslation =
        float4x4(float4(skyboxPc.mFrameBuffer->mCameraBuffer->mPerspective.mView[0].xyz, 0), float4(skyboxPc.mFrameBuffer->mCameraBuffer->mPerspective.mView[1].xyz, 0),
                 float4(skyboxPc.mFrameBuffer->mCameraBuffer->mPerspective.mView[2].xyz, 0), float4(0, 0, 0, 1));
    out.mPosition = mul(mul(skyboxPc.mFrameBuffer->mCameraBuffer->mPerspective.mProj, viewWithoutTranslation), position);
    out.mUvw = position.xyz;

    return out;
}
