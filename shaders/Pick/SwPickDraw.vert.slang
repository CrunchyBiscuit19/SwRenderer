import SwCommon;
import SwPick;

[[vk::push_constant]]
SwPick::DrawPC pickDrawPc;

SwPick::VSOutput main(SwPick::VSInput in) {
    SwPick::VSOutput out;

    SwVertex v = pickDrawPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderCommand rc = pickDrawPc.mDrawRcsBuffer[in.mDrawIndex];

    uint mSceneInstanceIndex = pickDrawPc.mSceneDrawRisIndicesBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = pickDrawPc.mSceneInstancesBuffer[mSceneInstanceIndex].mTransformMatrix;

    uint mSceneNodeTransformIndex = rc.mNodeTransformIndex;
    float4x4 nodeTransformMatrix = pickDrawPc.mSceneNodeTransformsBuffer[mSceneNodeTransformIndex];

    uint sceneMaterialIndex = rc.mMaterialIndex;
    SwMaterialConstant materialConstant = pickDrawPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

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
