import SwGeometry;
import SwCommon;

[[vk::binding(0, 0)]]
public uniform Sampler2D materialResources[];

[[vk::push_constant]]
SwGeometry::WorkPC workPc;

SwGeometry::VSOutput main(SwGeometry::VSInput in) {
    SwGeometry::VSOutput out;

    Sw::Vertex v = workPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    Sw::RenderCommand rc = workPc.mDrawRcsBuffer[in.mDrawIndex];

    uint sceneInstanceIndex = workPc.mSceneDrawRisIndicesBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = workPc.mSceneInstancesBuffer[sceneInstanceIndex].mTransformMatrix;

    uint sceneNodeTransformIndex = workPc.mDrawRcsBuffer[in.mDrawIndex].mNodeTransformIndex;
    float4x4 nodeTransformMatrix = workPc.mSceneNodeTransformsBuffer[sceneNodeTransformIndex];

    uint sceneMaterialIndex = workPc.mDrawRcsBuffer[in.mDrawIndex].mMaterialIndex;
    Sw::MaterialConstant materialConstant = workPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

    float4x4 modelMatrix = mul(instanceTransformMatrix, nodeTransformMatrix);
    float4 worldPosition = mul(modelMatrix, position);

    out.mPosition = mul(mul(workPc.mPerFrameBuffer->mPerspective.mProj, workPc.mPerFrameBuffer->mPerspective.mView), worldPosition);
    out.mWorldPos = worldPosition.xyz;
    out.mNormal = normalize(mul((float3x3)modelMatrix, v.mNormal));  // world-space normal; uniform scale only, so normalize absorbs the scale
    out.mTangent = float4(normalize(mul((float3x3)modelMatrix, v.mTangent.xyz)), v.mTangent.w);  // world-space tangent, keep handedness
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mMaterialIndex = sceneMaterialIndex;
    return out;
}
