import System.Geometry.SwGeometry;

[[vk::push_constant]]
SwGeometry::DrawPC drawPc;

SwGeometry::VSOutput main(SwGeometry::VSInput in) {
    SwGeometry::VSOutput out;

    SwVertex v = drawPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderCommand rc = drawPc.mSceneRcsBuffer[in.mDrawIndex];

    uint sceneInstanceIndex = drawPc.mSceneRisIndicesBuffer[in.mRenderItemIndex];
    float4x4 instanceTransformMatrix = drawPc.mSceneInstancesBuffer[sceneInstanceIndex].mTransformMatrix;

    uint sceneNodeTransformIndex = rc.mNodeTransformIndex;
    float4x4 nodeTransformMatrix = drawPc.mSceneNodeTransformsBuffer[sceneNodeTransformIndex];

    uint sceneMaterialIndex = rc.mMaterialIndex;
    SwMaterial::Constant materialConstant = drawPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

    float4x4 modelMatrix = mul(instanceTransformMatrix, nodeTransformMatrix);
    float4 worldPosition = mul(modelMatrix, position);

    out.mPosition = mul(mul(drawPc.mFrameBuffer->mCameraBuffer->mPerspective.mProj, drawPc.mFrameBuffer->mCameraBuffer->mPerspective.mView), worldPosition);
    out.mWorldPos = worldPosition.xyz;
    out.mNormal = normalize(mul((float3x3)modelMatrix, v.mNormal));  // world-space normal; uniform scale only, so normalize absorbs the scale
    out.mTangent = float4(mul((float3x3)modelMatrix, v.mTangent.xyz), v.mTangent.w);
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mMaterialIndex = sceneMaterialIndex;
    return out;
}
