import SwCommon;
import SwPick;

[[vk::push_constant]]
SwPick::DrawPC pickDrawPc;

SwPick::VSOutput computeVertex(SwPick::VSInput in) {
    SwPick::VSOutput out;

    Sw::Vertex v = pickDrawPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    Sw::RenderCommand rc = pickDrawPc.mDrawRcsBuffer[in.mDrawIndex];

    uint mSceneInstanceIndex = pickDrawPc.mSceneDrawRisIndicesBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = pickDrawPc.mSceneInstancesBuffer[mSceneInstanceIndex].mTransformMatrix;

    uint mSceneNodeTransformIndex = rc.mNodeTransformIndex;
    float4x4 nodeTransformMatrix = pickDrawPc.mSceneNodeTransformsBuffer[mSceneNodeTransformIndex];

    uint sceneMaterialIndex = rc.mMaterialIndex;
    Sw::MaterialConstant materialConstant = pickDrawPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

    out.mPosition =
        mul(mul(mul(mul(pickDrawPc.mPerFrameBuffer->mPerspective.mProj, pickDrawPc.mPerFrameBuffer->mPerspective.mView), instanceTransformMatrix),
                nodeTransformMatrix),
            position);
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mAlphaCutoff = materialConstant.mAlphaCutoff;
    out.mMaterialIndex = sceneMaterialIndex;
    out.mModelIndex = rc.mModelIndex;
    out.mInstanceIndex = mSceneInstanceIndex;

    return out;
}

[shader("vertex")]
SwPick::VSOutputOpaque mainOpaque(SwPick::VSInput in) {
    SwPick::VSOutput full = computeVertex(in);

    SwPick::VSOutputOpaque out;
    out.mPosition = full.mPosition;
    out.mModelIndex = full.mModelIndex;
    out.mInstanceIndex = full.mInstanceIndex;
    return out;
}

[shader("vertex")]
SwPick::VSOutput mainMasked(SwPick::VSInput in) {
    return computeVertex(in);
}
