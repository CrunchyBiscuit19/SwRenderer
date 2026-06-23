import SwCommon;
import SwLighting;

[[vk::push_constant]]
SwLighting::ShadowDrawPC shadowDrawPc;

SwLighting::VSOutput computeVertex(SwLighting::VSInput in) {
    SwLighting::VSOutput out;

    Sw::Vertex v = shadowDrawPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    Sw::RenderCommand rc = shadowDrawPc.mLightRcsBuffer[in.mDrawIndex];

    uint mSceneInstanceIndex = shadowDrawPc.mLightDrawRisIndicesBuffer[in.mInstanceIndex];
    float4x4 instanceTransformMatrix = shadowDrawPc.mSceneInstancesBuffer[mSceneInstanceIndex].mTransformMatrix;

    uint mSceneNodeTransformIndex = rc.mNodeTransformIndex;
    float4x4 nodeTransformMatrix = shadowDrawPc.mSceneNodeTransformsBuffer[mSceneNodeTransformIndex];

    uint sceneMaterialIndex = rc.mMaterialIndex;
    Sw::MaterialConstant materialConstant = shadowDrawPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

    float4x4 lightViewProj = shadowDrawPc.mPerFrameBuffer->mLightViewProj[shadowDrawPc.mLightIndex];
    float4x4 modelMatrix = mul(instanceTransformMatrix, nodeTransformMatrix);
    out.mPosition = mul(lightViewProj, mul(modelMatrix, position));
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mAlphaCutoff = materialConstant.mAlphaCutoff;
    out.mMaterialIndex = sceneMaterialIndex;
    out.mModelIndex = rc.mModelIndex;
    out.mInstanceIndex = mSceneInstanceIndex;

    return out;
}

[shader("vertex")]
SwLighting::VSOutputOpaque mainOpaque(SwLighting::VSInput in) {
    SwLighting::VSOutput full = computeVertex(in);

    SwLighting::VSOutputOpaque out;
    out.mPosition = full.mPosition;
    out.mModelIndex = full.mModelIndex;
    out.mInstanceIndex = full.mInstanceIndex;
    return out;
}

[shader("vertex")]
SwLighting::VSOutput mainMasked(SwLighting::VSInput in) {
    return computeVertex(in);
}
