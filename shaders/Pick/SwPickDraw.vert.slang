import SwCommon;
import SwPick;

[[vk::push_constant]]
SwPick::DrawPC pickDrawPc;

SwPick::VSOutput mScene(SwPick::VSInput in) {
    SwPick::VSOutput out;

    SwVertex v = pickDrawPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderItem renderItem = pickDrawPc.mPostCullRenderItemsBuffer[in.mDrawIndex];

    uint firstRenderInstance = renderItem.mFirstRenderInstance;
    uint mSceneInstanceIndex = pickDrawPc.mSceneVisibleRenderInstancesInstanceIndexBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = pickDrawPc.mSceneInstancesBuffer[mSceneInstanceIndex].mTransformMatrix;

    uint mSceneNodeTransformIndex = pickDrawPc.mPostCullRenderItemsBuffer[in.mDrawIndex].mNodeTransformIndex;
    float4x4 nodeTransformMatrix = pickDrawPc.mSceneNodeTransformsBuffer[mSceneNodeTransformIndex];

    out.mPosition =
        mul(mul(mul(mul(pickDrawPc.mPerFrameBuffer->mPerspective.mProj, pickDrawPc.mPerFrameBuffer->mPerspective.mView), instanceTransformMatrix),
                nodeTransformMatrix),
            position);
    out.mModelIndex = pickDrawPc.mPostCullRenderItemsBuffer[in.mDrawIndex].mModelIndex;
    out.mInstanceIndex = mSceneInstanceIndex;

    return out;
}
