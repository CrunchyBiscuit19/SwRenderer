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

    SwRenderCommand rc = workPc.mDrawRcsBuffer[in.mDrawIndex];

    uint sceneInstanceIndex = workPc.mSceneDrawRisIndicesBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = workPc.mSceneInstancesBuffer[sceneInstanceIndex].mTransformMatrix;

    uint sceneNodeTransformIndex = workPc.mDrawRcsBuffer[in.mDrawIndex].mNodeTransformIndex;
    float4x4 nodeTransformMatrix = workPc.mSceneNodeTransformsBuffer[sceneNodeTransformIndex];

    uint sceneMaterialIndex = workPc.mDrawRcsBuffer[in.mDrawIndex].mMaterialIndex;
    SwMaterialConstant materialConstant = workPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

    out.mPosition =
        mul(mul(mul(mul(workPc.mPerFrameBuffer->mPerspective.mProj, workPc.mPerFrameBuffer->mPerspective.mView), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.mNormal = v.mNormal;
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mAlphaCutoff = materialConstant.mAlphaCutoff;
    out.mMaterialIndex = sceneMaterialIndex;
    return out;
}
