import SwWBOIT;

SwWBOIT::VSOutput main(SwWBOIT::VSInput in) {
    SwWBOIT::VSOutput out;

    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };
    out.mPosition = float4(pos[in.mVertexIndex], 0.0, 1.0);

    return out;
}