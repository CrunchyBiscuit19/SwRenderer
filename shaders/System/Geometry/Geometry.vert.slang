import System.Geometry.SwGeometry;

[[vk::push_constant]]
SwGeometry::DrawPC drawPc;

SwGeometry::VSOutput main(SwGeometry::VSInput in) {
    SwGeometry::VSOutput out;

    SwVertex v = drawPc.mVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderCommand rc = drawPc.mRcsBuffer[in.mDrawIndex];

    uint sceneInstanceIndex = drawPc.mRisIndicesBuffer[in.mRenderItemIndex];
    float4x4 instanceTransformMatrix = drawPc.mInstancesBuffer[sceneInstanceIndex].mTransformMatrix;

    uint sceneNodeTransformIndex = rc.mNodeTransformIndex;
    float4x4 nodeTransformMatrix = drawPc.mNodeTransformsBuffer[sceneNodeTransformIndex];

    uint sceneMaterialIndex = rc.mMaterialIndex;
    SwMaterial::Constant materialConstant = drawPc.mMaterialConstantsBuffer[sceneMaterialIndex];

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
