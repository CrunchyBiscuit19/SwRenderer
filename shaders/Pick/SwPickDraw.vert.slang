import SwCommon;
import SwPick;

[[vk::push_constant]]
SwPick::DrawPC pickDrawPc;

SwPick::VSOutput main(SwPick::VSInput in) {
    SwPick::VSOutput out;

    SwVertex v = pickDrawPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderItem rItem = pickDrawPc.mDrawRItemsBuffer[in.mDrawIndex];

    uint firstRInst = rItem.mFirstRInst;
    uint mSceneInstanceIndex = pickDrawPc.mSceneVisibleRInstsIndicesBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = pickDrawPc.mSceneInstancesBuffer[mSceneInstanceIndex].mTransformMatrix;

    uint mSceneNodeTransformIndex = pickDrawPc.mDrawRItemsBuffer[in.mDrawIndex].mNodeTransformIndex;
    float4x4 nodeTransformMatrix = pickDrawPc.mSceneNodeTransformsBuffer[mSceneNodeTransformIndex];

    out.mPosition =
        mul(mul(mul(mul(pickDrawPc.mPerFrameBuffer->mPerspective.mProj, pickDrawPc.mPerFrameBuffer->mPerspective.mView), instanceTransformMatrix),
                nodeTransformMatrix),
            position);
    out.mModelIndex = pickDrawPc.mDrawRItemsBuffer[in.mDrawIndex].mModelIndex;
    out.mInstanceIndex = mSceneInstanceIndex;

    return out;
}
