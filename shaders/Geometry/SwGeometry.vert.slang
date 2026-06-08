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

    SwRenderItem rItem = workPc.mDrawRItemsBuffer[in.mDrawIndex];

    uint sceneInstanceIndex = workPc.mSceneDrawRInstsIndicesBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = workPc.mSceneInstancesBuffer[sceneInstanceIndex].mTransformMatrix;

    uint sceneNodeTransformIndex = workPc.mDrawRItemsBuffer[in.mDrawIndex].mNodeTransformIndex;
    float4x4 nodeTransformMatrix = workPc.mSceneNodeTransformsBuffer[sceneNodeTransformIndex];

    uint sceneMaterialIndex = workPc.mDrawRItemsBuffer[in.mDrawIndex].mMaterialIndex;
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
