import System.Lighting.SwLighting;

[[vk::push_constant]]
SwLighting::ShadowDrawPC shadowsDrawPc;

SwLighting::VSOutput computeVertex(SwLighting::VSInput in) {
    SwLighting::VSOutput out;

    SwVertex v = shadowsDrawPc.mVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwRenderCommand* rc = &shadowsDrawPc.mShadowsRcsBuffer[shadowsDrawPc.mRcsBase + in.mDrawIndex];

    uint shadowView = rc->mShadowView;
    uint layer = shadowView - shadowsDrawPc.mShadowViewBase;

    uint risIndex = shadowView * shadowsDrawPc.mNumRisPerShadowView + in.mRenderItemIndex;
    
    uint instanceIndex = shadowsDrawPc.mShadowsRisIndicesBuffer[risIndex];
    float4x4 instanceTransformMatrix = shadowsDrawPc.mInstancesBuffer[instanceIndex].mTransformMatrix;
    uint mNodeTransformIndex = rc->mNodeTransformIndex;
    float4x4 nodeTransformMatrix = shadowsDrawPc.mNodeTransformsBuffer[mNodeTransformIndex];

    uint sceneMaterialIndex = rc->mMaterialIndex;
    SwMaterial::Constant* materialConstant = &shadowsDrawPc.mMaterialConstantsBuffer[sceneMaterialIndex];

    float4x4 lightViewProj = shadowsDrawPc.mShadowsViewsBuffer[shadowView].mViewProj;
    float4x4 modelMatrix = mul(instanceTransformMatrix, nodeTransformMatrix);
    out.mLayer = layer;
    out.mPosition = mul(lightViewProj, mul(modelMatrix, position));
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant->mBaseFactor;
    out.mMaterialIndex = sceneMaterialIndex;

    return out;
}

[shader("vertex")]
SwLighting::VSOutput main(SwLighting::VSInput in) {
    return computeVertex(in);
}
