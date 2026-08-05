import System.Lighting.SwLighting;

[[vk::push_constant]]
SwLighting::ShadowDrawPC shadowsDrawPc;

SwLighting::VSOutput computeVertex(SwLighting::VSInput in) {
    SwLighting::VSOutput out;

    SwVertex v = shadowsDrawPc.mVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    uint layer = in.mDrawIndex / shadowsDrawPc.mNumRcsPerShadowView;
    uint absoluteView = shadowsDrawPc.mViewBase + layer;

    SwRenderCommand* rc = &shadowsDrawPc.mShadowsRcsBuffer[shadowsDrawPc.mViewBase * shadowsDrawPc.mNumRcsPerShadowView + in.mDrawIndex];

    uint risIndex = absoluteView * shadowsDrawPc.mNumRisPerShadowView + in.mRenderItemIndex;
    
    uint instanceIndex = shadowsDrawPc.mShadowsRisIndicesBuffer[risIndex];
    float4x4 instanceTransformMatrix = shadowsDrawPc.mInstancesBuffer[instanceIndex].mTransformMatrix;
    uint mNodeTransformIndex = rc->mNodeTransformIndex;
    float4x4 nodeTransformMatrix = shadowsDrawPc.mNodeTransformsBuffer[mNodeTransformIndex];

    uint sceneMaterialIndex = rc->mMaterialIndex;
    SwMaterial::Constant* materialConstant = &shadowsDrawPc.mMaterialConstantsBuffer[sceneMaterialIndex];

    float4x4 lightViewProj = shadowsDrawPc.mShadowsViewsBuffer[absoluteView].mViewProj;
    float4x4 modelMatrix = mul(instanceTransformMatrix, nodeTransformMatrix);
    out.mLayer = layer;
    out.mPosition = mul(lightViewProj, mul(modelMatrix, position));
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant->mBaseFactor;
    out.mAlphaCutoff = materialConstant->mAlphaCutoff;
    out.mMaterialIndex = sceneMaterialIndex;
    out.mAssetIndex = rc->mAssetIndex;
    out.mInstanceIndex = instanceIndex;

    return out;
}

[shader("vertex")]
SwLighting::VSOutput main(SwLighting::VSInput in) {
    return computeVertex(in);
}
