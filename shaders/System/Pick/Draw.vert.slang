import System.Pick.SwPick;

[[vk::push_constant]]
SwPick::DrawPC pickDrawPc;

SwPick::VSOutput computeVertex(SwPick::VSInput in) {
    SwPick::VSOutput out;

    SwVertex v = pickDrawPc.mVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderCommand rc = pickDrawPc.mRcsBuffer[in.mDrawIndex];

    uint mInstanceIndex = pickDrawPc.mRisIndicesBuffer[in.mRenderItemIndex];
    float4x4 instanceTransformMatrix = pickDrawPc.mInstancesBuffer[mInstanceIndex].mTransformMatrix;

    uint mNodeTransformIndex = rc.mNodeTransformIndex;
    float4x4 nodeTransformMatrix = pickDrawPc.mNodeTransformsBuffer[mNodeTransformIndex];

    uint sceneMaterialIndex = rc.mMaterialIndex;
    SwMaterial::Constant materialConstant = pickDrawPc.mMaterialConstantsBuffer[sceneMaterialIndex];

    out.mPosition =
        mul(mul(mul(mul(pickDrawPc.mFrameBuffer->mCameraBuffer->mPerspective.mProj, pickDrawPc.mFrameBuffer->mCameraBuffer->mPerspective.mView), instanceTransformMatrix),
                nodeTransformMatrix),
            position);
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mAlphaCutoff = materialConstant.mAlphaCutoff;
    out.mMaterialIndex = sceneMaterialIndex;
    out.mAssetIndex = rc.mAssetIndex;
    out.mInstanceIndex = mInstanceIndex;

    return out;
}

[shader("vertex")]
SwPick::VSOutputOpaque mainOpaque(SwPick::VSInput in) {
    SwPick::VSOutput full = computeVertex(in);

    SwPick::VSOutputOpaque out;
    out.mPosition = full.mPosition;
    out.mAssetIndex = full.mAssetIndex;
    out.mInstanceIndex = full.mInstanceIndex;
    return out;
}

[shader("vertex")]
SwPick::VSOutput mainMasked(SwPick::VSInput in) {
    return computeVertex(in);
}
