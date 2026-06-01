import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

[[vk::push_constant]]
SwGeometry::WorkPC workPc;

SwGeometry::VSOutput main(SwGeometry::VSInput in) {
    SwGeometry::VSOutput out;

    SwVertex v = workPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderItem renderItem = workPc.mPostCullRenderItemsBuffer[in.mDrawIndex];

    uint firstRenderInstance = renderItem.mFirstRenderInstance;
    uint sceneInstanceIndex = workPc.mSceneVisibleRenderInstancesInstanceIndexBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = workPc.mSceneInstancesBuffer[sceneInstanceIndex].mTransformMatrix;

    uint sceneNodeTransformIndex = workPc.mPostCullRenderItemsBuffer[in.mDrawIndex].mNodeTransformIndex;
    float4x4 nodeTransformMatrix = workPc.mSceneNodeTransformsBuffer[sceneNodeTransformIndex];

    uint sceneMaterialIndex = workPc.mPostCullRenderItemsBuffer[in.mDrawIndex].mMaterialIndex;
    SwMaterialConstant materialConstant = workPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

    out.mPosition =
        mul(mul(mul(mul(workPc.mPerFrameBuffer->mPerspective.mProj, workPc.mPerFrameBuffer->mPerspective.mView), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.mNormal = v.mNormal;
    out.mUv = float2(v.mUv.x, v.mUv.y);
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mMaterialIndex = sceneMaterialIndex;
    return out;
}
