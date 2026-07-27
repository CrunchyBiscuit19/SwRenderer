import System.Lighting.SwLighting;

[[vk::push_constant]]
SwLighting::ShadowDrawPC shadowsDrawPc;

SwLighting::VSOutput computeVertex(SwLighting::VSInput in) {
    SwLighting::VSOutput out;

    SwVertex v = shadowsDrawPc.mVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderCommand* rc = &shadowsDrawPc.mShadowsRcsBuffer[in.mDrawIndex];

    uint mInstanceIndex = shadowsDrawPc.mShadowsRisIndicesBuffer[in.mRenderItemIndex];
    float4x4 instanceTransformMatrix = shadowsDrawPc.mInstancesBuffer[mInstanceIndex].mTransformMatrix;

    uint mNodeTransformIndex = rc->mNodeTransformIndex;
    float4x4 nodeTransformMatrix = shadowsDrawPc.mNodeTransformsBuffer[mNodeTransformIndex];

    uint sceneMaterialIndex = rc->mMaterialIndex;
    SwMaterial::Constant* materialConstant = &shadowsDrawPc.mMaterialConstantsBuffer[sceneMaterialIndex];

    float4x4 activeLightViewProj = float4x4(1.f);  // TODO: source the rebuilt per-light view-projection (this pass is currently a no-op)
    float4x4 modelMatrix = mul(instanceTransformMatrix, nodeTransformMatrix);
    out.mPosition = mul(activeLightViewProj, mul(modelMatrix, position));
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant->mBaseFactor;
    out.mAlphaCutoff = materialConstant->mAlphaCutoff;
    out.mMaterialIndex = sceneMaterialIndex;
    out.mAssetIndex = rc->mAssetIndex;
    out.mInstanceIndex = mInstanceIndex;

    return out;
}

[shader("vertex")]
SwLighting::VSOutputOpaque mainOpaque(SwLighting::VSInput in) {
    SwLighting::VSOutput full = computeVertex(in);

    SwLighting::VSOutputOpaque out;
    out.mPosition = full.mPosition;
    out.mAssetIndex = full.mAssetIndex;
    out.mInstanceIndex = full.mInstanceIndex;
    return out;
}

[shader("vertex")]
SwLighting::VSOutput mainMasked(SwLighting::VSInput in) {
    return computeVertex(in);
}
