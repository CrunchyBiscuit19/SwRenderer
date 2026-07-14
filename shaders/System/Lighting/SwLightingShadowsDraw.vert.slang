import System.Lighting.SwLighting;

[[vk::push_constant]]
SwLighting::ShadowDrawPC shadowsDrawPc;

SwLighting::VSOutput computeVertex(SwLighting::VSInput in) {
    SwLighting::VSOutput out;

    SwMesh::Vertex v = shadowsDrawPc.mSceneVertexBuffer[in.mVertexIndex];
    float4 position = float4(v.mPosition, 1.0f);

    SwBatch::RenderCommand rc = shadowsDrawPc.mSceneShadowsRcsBuffer[in.mDrawIndex];

    uint mSceneInstanceIndex = shadowsDrawPc.mSceneShadowsRisIndicesBuffer[in.mRenderItemIndex];
    float4x4 instanceTransformMatrix = shadowsDrawPc.mSceneInstancesBuffer[mSceneInstanceIndex].mTransformMatrix;

    uint mSceneNodeTransformIndex = rc.mNodeTransformIndex;
    float4x4 nodeTransformMatrix = shadowsDrawPc.mSceneNodeTransformsBuffer[mSceneNodeTransformIndex];

    uint sceneMaterialIndex = rc.mMaterialIndex;
    SwMaterial::Constant materialConstant = shadowsDrawPc.mSceneMaterialConstantsBuffer[sceneMaterialIndex];

    float4x4 activeLightViewProj = float4x4(1.f);  // TODO: source the rebuilt per-light view-projection (this pass is currently a no-op)
    float4x4 modelMatrix = mul(instanceTransformMatrix, nodeTransformMatrix);
    out.mPosition = mul(activeLightViewProj, mul(modelMatrix, position));
    out.mUv = v.mUv;
    out.mColor = v.mColor * materialConstant.mBaseFactor;
    out.mAlphaCutoff = materialConstant.mAlphaCutoff;
    out.mMaterialIndex = sceneMaterialIndex;
    out.mAssetIndex = rc.mAssetIndex;
    out.mInstanceIndex = mSceneInstanceIndex;

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
